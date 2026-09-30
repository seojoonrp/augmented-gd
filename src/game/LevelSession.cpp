#include "LevelSession.hpp"
#include "AugmentManager.hpp"
#include "Language.hpp"
#include "Records.hpp"
#include "../augments/Augments.hpp"
#include "../ui/ProgressMarks.hpp"
#include "../ui/RunHud.hpp"

#include <Geode/binding/PlayLayer.hpp>

#include <algorithm>

using namespace geode::prelude;

namespace augment {

LevelSession::LevelSession(PlayLayer* layer, int levelID)
    : m_layer(layer), m_levelID(levelID), m_augments(makeAllAugments()) {}

LevelSession::~LevelSession() = default;

AugmentManager& LevelSession::mgr() const {
    return AugmentManager::get();
}

bool LevelSession::runLevel() const {
    return this->mgr().isRunFor(m_levelID);
}

bool LevelSession::runAttempt() const {
    return this->runLevel() && !m_layer->m_isPracticeMode && !m_layer->m_isTestMode;
}

float LevelSession::percent() const {
    return m_layer->getCurrentPercent();
}

float LevelSession::progress() const {
    return this->percent() / 100.f;
}

int LevelSession::levelOf(std::string_view id) const {
    return this->mgr().levelOf(id);
}

void LevelSession::notice(std::string const& text) {
    if (m_hud) m_hud->notice(text);
}

void LevelSession::banner(std::string const& text) {
    if (m_hud) m_hud->banner(text);
}

void LevelSession::rewardDeath(CCPoint at, float before, float cost, DeathResult const& r) {
    if (!m_hud) return;
    // all maxed: the bar stays MAX, no numbers or particles
    if (this->mgr().allMaxed()) {
        this->refreshHud(true);
        return;
    }
    m_hud->playDeathReward(at, before, r.charge - r.bonus, r.bonus, cost);
    this->refreshHud(true);
}

ProgressMarks* LevelSession::marks() const {
    return m_marks;
}

void LevelSession::setMarks(ProgressMarks* marks) {
    m_marks = marks;
}

namespace {
    // text rebuilds per second; the timers show one decimal, so 10 Hz is plenty
    constexpr float kHudRate = 10.f;
}

void LevelSession::tickHud(float dt) {
    m_hudClock += dt;
    this->refreshHud(false);
}

void LevelSession::refreshHud(bool force) {
    if (!m_hud) return;
    if (!force && m_hudClock < 1.f / kHudRate) return;
    m_hudClock = 0.f;
    auto& mgr = this->mgr();

    float now = this->percent();
    // the run best skips a checkpointed life until it settles, the readout
    // shouldn't slide back meanwhile
    float best = std::max({ now, mgr.bestPercent(), mgr.lifeBest() });
    // a waiting gauge draft shows as a full bar at its own cost, e.g. 30/30
    // (the threshold already moved on). not the opening draft, the gauge is really 0 then
    bool const draftWaiting = mgr.pendingGaugeDrafts() > 0;
    m_hud->setGauge(mgr.gauge(), draftWaiting ? mgr.lastDraftCost() : mgr.gaugeThreshold(), draftWaiting, mgr.allMaxed());
    if (m_marks) m_marks->setBest(best);

    // top-left readout is debug only (and can be off even then, for clean
    // screenshots). the pause menu shows the same
    if (!AugmentManager::debugMode() || !Mod::get()->getSettingValue<bool>("debug-readout")) {
        m_hud->setHeader("");
        m_hud->setSlots({});
        return;
    }
    // record = best of all runs on this level, not GD's
    m_hud->setHeader(fmt::format(
        "deaths {}   now {:.1f}%   best {:.1f}%   record {}%",
        mgr.deaths(), now, best, records::best(m_layer->m_level)
    ));

    // names follow the language setting, state text stays English
    std::vector<RunHud::Slot> slots;
    Lang const lang = language();
    for (auto const& def : allAugments()) {
        if (mgr.levelOf(def.id) <= 0) continue;
        auto a = this->find(def.id);
        slots.push_back({ def.name.in(lang), a ? a->hudState(*this, def.id) : std::string() });
    }
    m_hud->setSlots(slots);
}

void LevelSession::checkSectionFiling() {
    m_filingChecked = true;
    // first object past column 0 should sit in column floor(x / SectionWidth)
    auto const& columns = m_layer->m_sections;
    auto const& sizes = m_layer->m_sectionSizes;
    for (std::size_t i = 1; i < columns.size() && i < sizes.size(); i++) {
        auto column = columns[i];
        auto counts = sizes[i];
        if (!column || !counts) continue;
        for (std::size_t j = 0; j < column->size() && j < counts->size(); j++) {
            auto section = column->at(j);
            if (!section || counts->at(j) <= 0 || section->empty()) continue;
            auto obj = section->at(0);
            if (!obj) continue;
            float const x = obj->getPositionX();
            int const expected = static_cast<int>(std::floor(x / SectionWidth));
            if (expected != static_cast<int>(i)) {
                log::warn(
                    "Sections: object at x {:.0f} is in column {}, the scan expected {}",
                    x, i, expected
                );
            }
            return;
        }
    }
}

// ---------------------------------------------------------------- fan-out

void LevelSession::onLevelInit() {
    for (auto& a : m_augments) a->onLevelInit(*this);
}

void LevelSession::onLevelReady() {
    for (auto& a : m_augments) a->onLevelReady(*this);
}

void LevelSession::onObjectAdded(GameObject* obj) {
    for (auto& a : m_augments) a->onObjectAdded(*this, obj);
}

void LevelSession::onQuit() {
    for (auto& a : m_augments) a->onQuit(*this);
}

bool LevelSession::onBeforeReset() {
    bool resume = false;
    for (auto& a : m_augments) resume = a->onBeforeReset(*this) || resume;
    return resume;
}

void LevelSession::onAttemptStart(bool fromCheckpoint) {
    m_deathCounted = false;
    m_runCleared = false;
    // the reward animation belongs to the attempt that died
    if (m_hud) m_hud->settleGauge();
    for (auto& a : m_augments) a->onAttemptStart(*this, fromCheckpoint);
    if (fromCheckpoint) {
        for (auto& a : m_augments) a->onCheckpointRespawn(*this);
    }
}

void LevelSession::onCheckpointPlaced() {
    for (auto& a : m_augments) a->onCheckpointPlaced(*this);
}

bool LevelSession::onHit(PlayerObject* player, GameObject* object) {
    // free answers first (hitPriority), then table order
    for (int tier = 1; tier >= 0; tier--) {
        for (auto& a : m_augments) {
            if ((a->hitPriority() > 0) != (tier > 0)) continue;
            if (a->onHit(*this, player, object)) return true;
        }
    }
    return false;
}

void LevelSession::onHazardsDestroyed(int count) {
    if (count <= 0) return;
    for (auto& a : m_augments) a->onHazardsDestroyed(*this, count);
}

bool LevelSession::onDeath() {
    bool respawning = false;
    for (auto& a : m_augments) respawning = a->onDeath(*this) || respawning;
    return respawning;
}

void LevelSession::onFrame(float dt) {
    for (auto& a : m_augments) a->onFrame(*this, dt);
}

void LevelSession::onPause() {
    for (auto& a : m_augments) a->onPause(*this);
}

void LevelSession::onGranted(std::string const& id, int level) {
    for (auto& a : m_augments) a->onGranted(*this, id, level);
}

bool LevelSession::onHotkey(Hotkey which, bool down) {
    for (auto& a : m_augments) {
        if (a->onHotkey(*this, which, down)) return true;
    }
    return false;
}

Augment* LevelSession::find(std::string const& id) const {
    for (auto& a : m_augments) {
        if (a->handles(id)) return a.get();
    }
    return nullptr;
}

bool LevelSession::countDeath() {
    if (m_deathCounted) return false;
    m_deathCounted = true;
    return true;
}

// ---------------------------------------------------------------- debug keys

bool LevelSession::debugGrant(int index) {
    auto const& defs = allAugments();
    if (index < 0 || index >= static_cast<int>(defs.size())) return false;
    if (!this->runLevel()) return false;
    auto const& def = defs[index];
    if (this->levelOf(def.id) >= def.maxLevel) return true;
    int lvl = this->mgr().grant(def.id);
    this->onGranted(def.id, lvl);
    return true;
}

bool LevelSession::debugFillGauge() {
    if (!this->runLevel()) return false;
    this->mgr().debugFillGauge();
    return true;
}

} // namespace augment

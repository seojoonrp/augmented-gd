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
    // Nothing left to draft: the bar stays a full MAX, no numbers or
    // particles flying into it.
    if (this->mgr().allMaxed()) {
        log::info("Death reward skipped: every augment is maxed");
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
    // HUD text rebuilds per second. The rows carry timers with one decimal,
    // so 10 Hz reads smoothly; per frame was ~10 fmt strings a frame for
    // nothing.
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
    // lifeBest is the furthest a life held open by a checkpoint got to: the
    // run's best does not count it until the life settles, but the readout
    // and the mark must not slide back to the checkpoint in the meantime.
    float best = std::max({ now, mgr.bestPercent(), mgr.lifeBest() });
    // A gauge-earned draft that is still waiting shows as a full bar reading
    // its own cost, e.g. 30/30 (the threshold has already moved on); the free
    // opening draft does not (the gauge really is at 0 then).
    // Once every augment is maxed the bar is simply full and reads MAX.
    bool const draftWaiting = mgr.pendingGaugeDrafts() > 0;
    m_hud->setGauge(mgr.gauge(), draftWaiting ? mgr.lastDraftCost() : mgr.gaugeThreshold(), draftWaiting, mgr.allMaxed());
    if (m_marks) m_marks->setBest(best);

    // The top-left readout (header + one row per augment) is debug-mode only
    // since the pause menu shows the same things (user, 2026-09-28).
    if (!AugmentManager::debugMode()) {
        m_hud->setHeader("");
        m_hud->setSlots({});
        return;
    }
    // `record` is every run's best on this level (Records.hpp), apart from
    // both this run's best and GD's normal one.
    m_hud->setHeader(fmt::format(
        "deaths {}   now {:.1f}%   best {:.1f}%   record {}%",
        mgr.deaths(), now, best, records::best(m_layer->m_level)
    ));

    // One row per owned augment in table order. Names are the Korean
    // display names; the state text stays English (debug readout).
    std::vector<RunHud::Slot> slots;
    Lang const lang = language();
    for (auto const& def : allAugments()) {
        if (mgr.levelOf(def.id) <= 0) continue;
        auto a = this->find(def.id);
        slots.push_back({ def.name.in(lang), a ? a->hudState(*this, def.id) : std::string() });
    }
    m_hud->setSlots(slots);
}

std::vector<GameObject*>& LevelSession::objectsByX() {
    if (m_objectsByX.empty() && m_layer->m_objects) {
        // Each x is read once up front: sorting on getPositionX() itself
        // costs two virtual calls per comparison, ~n log n of them.
        std::vector<std::pair<float, GameObject*>> keyed;
        keyed.reserve(m_layer->m_objects->count());
        for (auto obj : CCArrayExt<GameObject*>(m_layer->m_objects)) {
            if (obj->m_objectType == GameObjectType::Decoration || obj->m_isDecoration) continue;
            keyed.emplace_back(obj->getPositionX(), obj);
        }
        std::sort(keyed.begin(), keyed.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
        m_objectsByX.reserve(keyed.size());
        for (auto const& entry : keyed) m_objectsByX.push_back(entry.second);
        log::info("Tracking {} non-decoration objects by x", m_objectsByX.size());
    }
    return m_objectsByX;
}

void LevelSession::warmObjectIndex() {
    // The augments that scan objectsByX().
    constexpr char const* kScanners[] = { ids::Foresight, ids::Cat, ids::Missile };
    if (!m_objectsByX.empty() || !this->runLevel()) return;
    if (std::ranges::none_of(kScanners, [&](char const* id) { return this->owns(id); })) return;
    this->objectsByX();
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
    if (m_deathCounted) {
        auto since = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_deathAt).count();
        log::info("Attempt start {:.2f} s after the death", since);
    }
    m_deathCounted = false;
    m_runCleared = false;
    // The reward sequence belongs to the attempt that died.
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
    // Augments that can answer the hit for free go first (Augment::hitPriority),
    // then the rest in table order.
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
    // A pick lands behind the draft popup; a scanner drafted now indexes
    // the level there instead of on its first sweep.
    this->warmObjectIndex();
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
    m_deathAt = std::chrono::steady_clock::now();
    return true;
}

// ---------------------------------------------------------------- debug keys

bool LevelSession::debugGrant(int index) {
    auto const& defs = allAugments();
    if (index < 0 || index >= static_cast<int>(defs.size())) {
        // Says out loud that the key hit nothing: a debug key for an
        // augment the running build does not have (a stale install) is
        // otherwise silent.
        log::info("Debug grant: no augment at index {} ({} in the table)", index, defs.size());
        return false;
    }
    if (!this->runLevel()) {
        log::info("Debug grant ignored: not a run level");
        return false;
    }
    auto const& def = defs[index];
    if (this->levelOf(def.id) >= def.maxLevel) {
        log::info("Debug grant: '{}' already maxed", def.id);
        return true;
    }
    int lvl = this->mgr().grant(def.id);
    log::info("Debug grant: '{}' -> level {}", def.id, lvl);
    this->onGranted(def.id, lvl);
    return true;
}

bool LevelSession::debugFillGauge() {
    if (!this->runLevel()) {
        log::info("Debug fill ignored: not a run level");
        return false;
    }
    this->mgr().debugFillGauge();
    return true;
}

} // namespace augment

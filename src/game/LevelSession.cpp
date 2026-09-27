#include "LevelSession.hpp"
#include "AugmentManager.hpp"
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

int LevelSession::levelOf(std::string const& id) const {
    return this->mgr().levelOf(id);
}

void LevelSession::notice(std::string const& text, ccColor3B color) {
    if (m_hud) m_hud->notice(text, color);
}

void LevelSession::rewardDeath(CCPoint at, float before, float cost, DeathResult const& r) {
    if (!m_hud) return;
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
    // A gauge-earned draft that is still waiting shows as a full bar with
    // DRAFT!; the free opening draft does not (the gauge really is at 0 then).
    m_hud->setGauge(mgr.gauge(), mgr.gaugeThreshold(), mgr.pendingGaugeDrafts() > 0);
    m_hud->setHeader(fmt::format("deaths {}   now {:.1f}%   best {:.1f}%", mgr.deaths(), now, best));
    if (m_marks) m_marks->setBest(best);

    // One row per owned augment in table order. Names are the Korean
    // display names; the state text stays English (debug readout).
    std::vector<RunHud::Slot> slots;
    for (auto const& def : allAugments()) {
        if (mgr.levelOf(def.id) <= 0) continue;
        auto a = this->find(def.id);
        slots.push_back({ def.name, a ? a->hudState(*this, def.id) : std::string() });
    }
    m_hud->setSlots(slots);
}

std::vector<GameObject*>& LevelSession::objectsByX() {
    if (m_objectsByX.empty() && m_layer->m_objects) {
        for (auto obj : CCArrayExt<GameObject*>(m_layer->m_objects)) {
            if (obj->m_objectType == GameObjectType::Decoration || obj->m_isDecoration) continue;
            m_objectsByX.push_back(obj);
        }
        std::sort(m_objectsByX.begin(), m_objectsByX.end(), [](GameObject* a, GameObject* b) {
            return a->getPositionX() < b->getPositionX();
        });
        log::info("Tracking {} non-decoration objects by x", m_objectsByX.size());
    }
    return m_objectsByX;
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
        this->notice(fmt::format("{} MAXED", def.name), { 255, 120, 120 });
        return true;
    }
    int lvl = this->mgr().grant(def.id);
    log::info("Debug grant: '{}' -> level {}", def.id, lvl);
    this->onGranted(def.id, lvl);
    this->notice(fmt::format("+{} Lv{} (DEBUG)", def.name, lvl), { 200, 160, 255 });
    return true;
}

bool LevelSession::debugFillGauge() {
    if (!this->runLevel()) {
        log::info("Debug fill ignored: not a run level");
        return false;
    }
    float added = this->mgr().debugFillGauge();
    this->notice(fmt::format("GAUGE FULL +{:.0f} (DEBUG)", added), { 200, 160, 255 });
    return true;
}

} // namespace augment

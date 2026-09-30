#include "AugmentManager.hpp"
#include "DraftSession.hpp"
#include "LevelSession.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayLayer.hpp>

#include <random>

using namespace geode::prelude;

namespace augment {

AugmentManager& AugmentManager::get() {
    static AugmentManager instance;
    return instance;
}

AugmentManager::AugmentManager() = default;
AugmentManager::~AugmentManager() = default;

LevelSession& AugmentManager::beginLevel(PlayLayer* layer, int levelID) {
    if (m_session) log::warn("beginLevel: replacing a session that never got onQuit");
    m_session = std::make_unique<LevelSession>(layer, levelID);
    return *m_session;
}

void AugmentManager::endLevel() {
    m_session.reset();
}

LevelSession* AugmentManager::session() const {
    return this->sessionFor(PlayLayer::get());
}

LevelSession* AugmentManager::sessionFor(PlayLayer* layer) const {
    if (!layer || !m_session || m_session->layer() != layer) return nullptr;
    return m_session.get();
}

void AugmentManager::startRun(GJGameLevel* level) {
    draft::abandon();
    m_state.start(level->m_levelID.value(), std::string(level->m_levelName));
    log::info("Run started on '{}' ({})", m_state.levelName(), m_state.levelID());
}

void AugmentManager::armRunEntry(int levelID) {
    m_armedLevel = levelID;
}

void AugmentManager::disarmRunEntry() {
    m_armedLevel = 0;
}

bool AugmentManager::takeRunEntry(int levelID) {
    bool const armed = m_armedLevel != 0 && m_armedLevel == levelID;
    m_armedLevel = 0;
    return armed;
}

void AugmentManager::endRun() {
    draft::abandon();
    if (!m_state.active()) return;

    log::info(
        "Run ended on '{}': {} deaths, {} drafts, {} augments",
        m_state.levelName(), m_state.deaths(), m_state.draftsTaken(), m_state.augments().size()
    );
    m_state.end();
}

bool AugmentManager::debugMode() {
    return Mod::get()->getSettingValue<bool>("debug-mode");
}

GaugeRule AugmentManager::gaugeRule() const {
    // same ramp in debug mode, so testing uses the real economy
    return GaugeRule{};
}

DeathResult AugmentManager::onDeath(float percent, bool respawning) {
    if (!m_state.active()) return {};
    auto r = m_state.onDeath(percent, this->gaugeRule(), respawning);
    if (r.deferred) {
        log::info("Death #{} at {:.1f}% (checkpoint, charge deferred)", m_state.deaths(), percent);
        return r;
    }
    // the charge is for the life's best, which can be above `percent`
    log::info(
        "Death #{} at {:.1f}%: +{:.0f} +{:.0f} best, gauge {:.0f}/{:.0f}, {} earned",
        m_state.deaths(), percent, r.charge - r.bonus, r.bonus, m_state.gauge(), this->gaugeThreshold(), r.earned
    );
    return r;
}

float AugmentManager::debugFillGauge() {
    return m_state.fillGauge(this->gaugeRule());
}

std::vector<AugmentDef const*> AugmentManager::rollDraft(size_t count) const {
    static std::mt19937 rng{ std::random_device{}() };
    return m_state.rollDraft(count, rng);
}

void AugmentManager::applyPick(std::string const& id) {
    if (int lvl = m_state.applyPick(id)) {
        log::info("Picked '{}' -> Lv{}", id, lvl);
    }
    else {
        log::warn("applyPick: unknown augment id '{}'", id);
    }
}

int AugmentManager::grant(std::string const& id) {
    int lvl = m_state.grant(id);
    if (!lvl) log::warn("grant: unknown augment id '{}'", id);
    return lvl;
}

} // namespace augment

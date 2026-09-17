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
    if (m_session) log::warn("beginLevel: replacing a session that never saw onQuit");
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
    log::info("Run started on '{}' (id {}), opening draft queued", m_state.levelName(), m_state.levelID());
}

void AugmentManager::endRun() {
    draft::abandon();
    if (!m_state.active()) return;

    log::info(
        "Run ended on '{}' after {} deaths, {} drafts, {} augments",
        m_state.levelName(), m_state.deaths(), m_state.draftsTaken(), m_state.augments().size()
    );
    m_state.end();
}

bool AugmentManager::debugMode() {
    return Mod::get()->getSettingValue<bool>("debug-mode");
}

GaugeRule AugmentManager::gaugeRule() const {
    GaugeRule rule;
    if (debugMode()) {
        rule.fixedThreshold = std::max(1.f, static_cast<float>(Mod::get()->getSettingValue<int64_t>("debug-threshold")));
    }
    return rule;
}

float AugmentManager::onDeath(float percent) {
    if (!m_state.active()) return 0.f;
    auto r = m_state.onDeath(percent, this->gaugeRule());
    log::info(
        "Death #{} at {:.1f}% -> +{:.0f} (+{:.0f} new best), gauge {:.0f}/{:.0f}, {} draft(s) earned, {} pending",
        m_state.deaths(), percent, percent, r.bonus, m_state.gauge(), this->gaugeThreshold(), r.earned, m_state.pendingDrafts()
    );
    return r.bonus;
}

float AugmentManager::debugFillGauge() {
    float added = m_state.fillGauge(this->gaugeRule());
    if (m_state.active()) {
        log::info("Debug fill: +{:.0f}, gauge {:.0f}/{:.0f}", added, m_state.gauge(), this->gaugeThreshold());
    }
    return added;
}

std::vector<AugmentDef const*> AugmentManager::rollDraft(size_t count) const {
    static std::mt19937 rng{ std::random_device{}() };
    return m_state.rollDraft(count, rng);
}

void AugmentManager::applyPick(std::string const& id) {
    if (int lvl = m_state.applyPick(id)) {
        log::info("Picked '{}' -> level {}", id, lvl);
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

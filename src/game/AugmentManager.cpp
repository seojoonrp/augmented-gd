#include "AugmentManager.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>

#include <random>

using namespace geode::prelude;

namespace augment {

AugmentManager& AugmentManager::get() {
    static AugmentManager instance;
    return instance;
}

void AugmentManager::startRun(GJGameLevel* level) {
    this->resumeGameAfterDraft();
    m_state.start(level->m_levelID.value(), std::string(level->m_levelName));
    log::info("Run started on '{}' (id {}), opening draft queued", m_state.levelName(), m_state.levelID());
}

void AugmentManager::endRun() {
    this->resumeGameAfterDraft();
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

void AugmentManager::pauseGameForDraft() {
    if (m_directorPaused) return;
    auto director = CCDirector::get();
    // CCDirector::pause() saves the current interval and drops to 4 fps;
    // put the real interval back so the popup stays responsive.
    // resume() restores the saved value, so nothing extra is needed there.
    auto interval = director->getAnimationInterval();
    director->pause();
    director->setAnimationInterval(interval);
    m_directorPaused = true;
}

void AugmentManager::resumeGameAfterDraft() {
    if (!m_directorPaused) return;
    CCDirector::get()->resume();
    m_directorPaused = false;
}

} // namespace augment

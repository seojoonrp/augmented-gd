#include "AugmentManager.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>

#include <algorithm>
#include <random>

using namespace geode::prelude;

namespace augment {

AugmentManager& AugmentManager::get() {
    static AugmentManager instance;
    return instance;
}

void AugmentManager::startRun(GJGameLevel* level) {
    this->resumeGameAfterDraft();

    m_active = true;
    m_levelID = level->m_levelID.value();
    m_levelName = std::string(level->m_levelName);
    m_deaths = 0;
    m_deathsSinceDraft = 0;
    m_draftsTaken = 0;
    m_bestPercent = 0.f;
    m_pendingDraft = false;
    m_levels.clear();

    log::info("Run started on '{}' (id {})", m_levelName, m_levelID);
}

void AugmentManager::endRun() {
    this->resumeGameAfterDraft();
    if (!m_active) return;

    log::info(
        "Run ended on '{}' after {} deaths, {} drafts, {} augments",
        m_levelName, m_deaths, m_draftsTaken, m_levels.size()
    );
    m_active = false;
    m_pendingDraft = false;
}

int AugmentManager::deathsPerDraft() const {
    auto v = static_cast<int>(Mod::get()->getSettingValue<int64_t>("deaths-per-draft"));
    return std::max(1, v);
}

void AugmentManager::onDeath(float percent) {
    if (!m_active) return;

    m_deaths++;
    m_deathsSinceDraft++;
    m_bestPercent = std::max(m_bestPercent, percent);

    // TODO: draft condition is still undecided (attempt count / percent / ...).
    // For now: every N deaths, while there is still something left to draft.
    if (m_deathsSinceDraft >= this->deathsPerDraft() && !this->rollDraft(1).empty()) {
        m_deathsSinceDraft = 0;
        m_pendingDraft = true;
        log::info("Draft pending (death #{}, {:.1f}%)", m_deaths, percent);
    }
}

int AugmentManager::levelOf(std::string const& id) const {
    auto it = m_levels.find(id);
    return it == m_levels.end() ? 0 : it->second;
}

std::vector<AugmentDef const*> AugmentManager::rollDraft(size_t count) const {
    std::vector<AugmentDef const*> candidates;
    for (auto const& def : allAugments()) {
        if (this->levelOf(def.id) < def.maxLevel) candidates.push_back(&def);
    }

    static std::mt19937 rng{ std::random_device{}() };
    std::shuffle(candidates.begin(), candidates.end(), rng);
    if (candidates.size() > count) candidates.resize(count);
    return candidates;
}

void AugmentManager::applyPick(std::string const& id) {
    auto def = findAugment(id);
    if (!def) {
        log::warn("applyPick: unknown augment id '{}'", id);
        return;
    }
    int& lvl = m_levels[id];
    lvl = std::min(lvl + 1, def->maxLevel);
    m_draftsTaken++;
    log::info("Picked '{}' -> level {}", def->name, lvl);
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

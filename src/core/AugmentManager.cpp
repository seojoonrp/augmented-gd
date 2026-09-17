#include "AugmentManager.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>

#include <iterator>
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
    m_draftsTaken = 0;
    m_gaugeDrafts = 0;
    m_bestPercent = 0.f;
    m_gauge = 0.f;
    // Every run opens with a free draft; PlayLayer::startGame shows it. Not
    // gauge-earned, so it leaves the threshold ramp alone.
    m_pendingDrafts = 1;
    m_levels.clear();
    m_slowMoEnabled = true;

    log::info("Run started on '{}' (id {}), opening draft queued", m_levelName, m_levelID);
}

float AugmentManager::debugFillGauge() {
    if (!m_active) return 0.f;
    float missing = std::max(0.f, this->gaugeThreshold() - m_gauge);
    m_gauge += missing;
    log::info("Debug fill: +{:.0f}, gauge {:.0f}/{:.0f}", missing, m_gauge, this->gaugeThreshold());
    return missing;
}

void AugmentManager::endRun() {
    this->resumeGameAfterDraft();
    if (!m_active) return;

    log::info(
        "Run ended on '{}' after {} deaths, {} drafts, {} augments",
        m_levelName, m_deaths, m_draftsTaken, m_levels.size()
    );
    m_active = false;
    m_pendingDrafts = 0;
}

bool AugmentManager::debugMode() {
    return Mod::get()->getSettingValue<bool>("debug-mode");
}

float AugmentManager::gaugeThreshold() const {
    if (debugMode()) {
        return std::max(1.f, static_cast<float>(Mod::get()->getSettingValue<int64_t>("debug-threshold")));
    }
    return tune::GaugeThresholdStart + tune::GaugeThresholdStep * m_gaugeDrafts;
}

float AugmentManager::onDeath(float percent) {
    if (!m_active) return 0.f;

    m_deaths++;

    // No floor: dying at 3 % is worth 3, so farming early deaths never pays.
    // New ground is paid twice (at mult 1): the bonuses over a whole run sum
    // to at most 100 * mult, so this rewards progress and nothing else.
    float bonus = 0.f;
    if (percent > m_bestPercent) {
        bonus = (percent - m_bestPercent) * tune::NewBestBonusMult;
        m_bestPercent = percent;
    }
    m_gauge += percent + bonus;

    // A big new best can pay for several drafts at once; each one raises
    // the cost of the next. Leftover charge carries over.
    int earned = 0;
    while (m_gauge >= this->gaugeThreshold() && !this->rollDraft(1).empty()) {
        m_gauge -= this->gaugeThreshold();
        m_gaugeDrafts++;
        m_pendingDrafts++;
        earned++;
    }
    log::info(
        "Death #{} at {:.1f}% -> +{:.0f} (+{:.0f} new best), gauge {:.0f}/{:.0f}, {} draft(s) earned, {} pending",
        m_deaths, percent, percent, bonus, m_gauge, this->gaugeThreshold(), earned, m_pendingDrafts
    );
    return bonus;
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
    if (int lvl = this->grant(id)) {
        m_draftsTaken++;
        log::info("Picked '{}' -> level {}", id, lvl);
    }
}

int AugmentManager::grant(std::string const& id) {
    auto def = findAugment(id);
    if (!def) {
        log::warn("grant: unknown augment id '{}'", id);
        return 0;
    }
    int& lvl = m_levels[id];
    lvl = std::min(lvl + 1, def->maxLevel);
    return lvl;
}

float AugmentManager::slowMoScale() const {
    return 1.f - tune::SlowMoStep * this->levelOf(ids::SlowMo);
}

size_t AugmentManager::draftCardCount() const {
    return static_cast<size_t>(
        this->has(ids::DraftCount) ? tune::DraftCountCards : tune::DefaultDraftCards
    );
}

float AugmentManager::nerveBoost(float progress) const {
    int lvl = this->levelOf(ids::Nerve);
    if (lvl <= 0) return 1.f;
    // NerveMult has one entry per nerve level; clamp in case maxLevel grows
    // before the table does.
    int idx = std::clamp(lvl, 1, static_cast<int>(std::size(tune::NerveMult))) - 1;
    return 1.f + tune::NerveMult[idx] * std::clamp(progress, 0.f, 1.f);
}

// shrink is "how much is cut off", so 0 = untouched.
static float shrinkToScale(float shrink) {
    return std::clamp(1.f - shrink, tune::MinHitboxScale, 1.f);
}

float AugmentManager::hazardScale(float progress) const {
    float shrink = tune::HazardStep * this->levelOf(ids::HazardHitbox);
    return shrinkToScale(shrink * this->nerveBoost(progress));
}

float AugmentManager::waveScale(float progress) const {
    float shrink = tune::WaveStep * this->levelOf(ids::WaveHitbox);
    return shrinkToScale(shrink * this->nerveBoost(progress));
}

int AugmentManager::catCount() const {
    int lvl = this->levelOf(ids::Cat);
    if (lvl <= 0) return 0;
    return tune::CatBaseCount + tune::CatCountStep * (lvl - 1);
}

float AugmentManager::catInterval() const {
    int lvl = this->levelOf(ids::Cat);
    if (lvl <= 0) return 0.f;
    // Keep a sane floor should maxLevel ever outgrow the step table.
    return std::max(0.5f, tune::CatBaseInterval - tune::CatIntervalStep * (lvl - 1));
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

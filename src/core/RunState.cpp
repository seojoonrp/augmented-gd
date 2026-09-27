#include "RunState.hpp"
#include "Formulas.hpp"

#include <algorithm>
#include <cmath>

namespace augment {

void RunState::start(int levelID, std::string levelName) {
    m_active = true;
    m_levelID = levelID;
    m_levelName = std::move(levelName);
    m_deaths = 0;
    m_draftsTaken = 0;
    m_gaugeDrafts = 0;
    m_bestPercent = 0.f;
    m_lifeBest = 0.f;
    m_gauge = 0.f;
    // Every run opens with a free draft; PlayLayer::startGame shows it. Not
    // gauge-earned, so it leaves the threshold ramp alone.
    m_pendingDrafts = 1;
    m_pendingGaugeDrafts = 0;
    m_levels.clear();
    m_slowMoEnabled = true;
}

void RunState::end() {
    m_active = false;
    m_pendingDrafts = 0;
    m_pendingGaugeDrafts = 0;
    m_lifeBest = 0.f;
}

float RunState::gaugeThreshold(GaugeRule const& rule) const {
    float cost = rule.thresholdStart + rule.thresholdStep * static_cast<float>(m_gaugeDrafts);
    return std::min(cost, rule.thresholdMax);
}

DeathResult RunState::onDeath(float percent, GaugeRule const& rule, bool respawning) {
    DeathResult r;
    if (!m_active) return r;

    m_deaths++;

    // A death the run comes back from (checkpoint respawn) settles nothing:
    // the life is one visit to the level, so it pays once, for the furthest
    // point it reached. The best percent is left alone too, or the bonus
    // below would already be spent when the life ends.
    m_lifeBest = std::max(m_lifeBest, percent);
    if (respawning) {
        r.deferred = true;
        return r;
    }
    // Everything this life reached, including the deaths it was revived from.
    percent = m_lifeBest;
    m_lifeBest = 0.f;

    // No floor: dying at 3 % is worth 3, so farming early deaths never pays.
    // New ground is paid twice (at mult 1): the bonuses over a whole run sum
    // to at most 100 * mult, so this rewards progress and nothing else. A
    // new best counts in whole percents (4.1 -> 4.4 is not one: a "NEW
    // BEST +0" annoyed the user), while the best itself keeps the decimals
    // for the HUD and the progress dot.
    float wholeNew = std::floor(percent) - std::floor(m_bestPercent);
    if (wholeNew > 0.f) r.bonus = wholeNew * rule.newBestMult;
    m_bestPercent = std::max(m_bestPercent, percent);
    r.charge = percent + r.bonus;
    m_gauge += r.charge;

    // A big new best can pay for several drafts at once; each one raises
    // the cost of the next. Leftover charge carries over.
    while (m_gauge >= this->gaugeThreshold(rule) && this->anyDraftable()) {
        m_gauge -= this->gaugeThreshold(rule);
        m_gaugeDrafts++;
        m_pendingDrafts++;
        m_pendingGaugeDrafts++;
        r.earned++;
    }
    return r;
}

float RunState::fillGauge(GaugeRule const& rule) {
    if (!m_active) return 0.f;
    float missing = std::max(0.f, this->gaugeThreshold(rule) - m_gauge);
    m_gauge += missing;
    return missing;
}

void RunState::takePendingDraft() {
    if (m_pendingDrafts > 0) m_pendingDrafts--;
    if (m_pendingGaugeDrafts > m_pendingDrafts) m_pendingGaugeDrafts = m_pendingDrafts;
}

void RunState::dropPendingDrafts() {
    m_pendingDrafts = 0;
    m_pendingGaugeDrafts = 0;
}

int RunState::levelOf(std::string const& id) const {
    auto it = m_levels.find(id);
    return it == m_levels.end() ? 0 : it->second;
}

bool RunState::anyDraftable() const {
    for (auto const& def : allAugments()) {
        if (this->levelOf(def.id) < def.maxLevel) return true;
    }
    return false;
}

std::vector<AugmentDef const*> RunState::rollDraft(std::size_t count, std::mt19937& rng) const {
    std::vector<AugmentDef const*> candidates;
    for (auto const& def : allAugments()) {
        if (this->levelOf(def.id) < def.maxLevel) candidates.push_back(&def);
    }
    std::shuffle(candidates.begin(), candidates.end(), rng);
    if (candidates.size() > count) candidates.resize(count);
    return candidates;
}

std::size_t RunState::draftCardCount() const {
    return formula::draftCardCount(this->has(ids::DraftCount));
}

int RunState::grant(std::string const& id) {
    auto def = findAugment(id);
    if (!def) return 0;
    int& lvl = m_levels[id];
    lvl = std::min(lvl + 1, def->maxLevel);
    return lvl;
}

int RunState::applyPick(std::string const& id) {
    int lvl = this->grant(id);
    if (lvl) m_draftsTaken++;
    return lvl;
}

float RunState::slowMoScale() const {
    return formula::slowMoScale(this->levelOf(ids::SlowMo));
}

float RunState::nerveBoost(float progress) const {
    return formula::nerveBoost(this->levelOf(ids::Nerve), progress);
}

float RunState::hazardScale(float progress) const {
    return formula::hazardScale(this->levelOf(ids::HazardHitbox), this->levelOf(ids::Nerve), progress);
}

float RunState::waveScale(float progress) const {
    return formula::waveScale(this->levelOf(ids::WaveHitbox), this->levelOf(ids::Nerve), progress);
}

int RunState::catCount() const {
    return formula::catCount(this->levelOf(ids::Cat));
}

float RunState::catInterval() const {
    return formula::catInterval(this->levelOf(ids::Cat));
}

float RunState::missileInterval() const {
    return formula::missileInterval(this->levelOf(ids::Missile));
}

float RunState::missileRadius() const {
    return formula::missileRadius(this->levelOf(ids::Missile));
}

float RunState::berserkChance() const {
    return formula::berserkChance(this->levelOf(ids::Berserk));
}

float RunState::berserkSeconds() const {
    return formula::berserkSeconds(this->levelOf(ids::Berserk));
}

} // namespace augment

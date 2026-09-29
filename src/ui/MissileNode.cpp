#include "MissileNode.hpp"

#include <algorithm>
#include <cmath>
#include <random>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kBodyLength = 22.f;
    constexpr float kBodyWidth = 3.f;
    constexpr float kTrailLength = 40.f;
    constexpr float kReticleWidth = 1.f;
    constexpr int kSegments = 48;

    // The blast, in seconds from impact and fractions of the blast radius.
    // White is the base (user 2026-09-27); the layers are told apart by
    // alpha and timing: a flash over the cleared circle, a fireball that
    // swells and collapses, a fast shockwave ring past the radius with a
    // fainter one behind it, and sparks thrown out that arc down. Red is
    // only an accent (user 2026-09-29, "never too much"): the fireball's
    // soft halo, and the sparks cooling to it as they fade.
    constexpr float kAccentR = 1.f;
    constexpr float kAccentG = 0.2f;
    constexpr float kAccentB = 0.15f;
    constexpr float kFlashSeconds = 0.12f;
    constexpr float kCoreGrowSeconds = 0.07f;
    constexpr float kCoreSeconds = 0.32f;
    constexpr float kCoreRadius = 0.36f;
    constexpr float kWaveSeconds = 0.4f;
    constexpr float kWaveReach = 1.3f;
    constexpr float kEchoDelay = 0.06f;
    constexpr float kEchoSeconds = 0.36f;
    constexpr float kEchoReach = 0.95f;
    // A spark's streak = where it was this long ago.
    constexpr float kSparkLag = 0.045f;
    constexpr float kSparkGravity = 300.f;

    // CCDrawNode wants premultiplied alpha.
    ccColor4F premul(float r, float g, float b, float a) {
        return { r * a, g * a, b * a, a };
    }
    ccColor4F white(float a) { return premul(1.f, 1.f, 1.f, a); }
    // White at 0, the accent red at 1.
    ccColor4F toAccent(float mix, float a) {
        return premul(
            1.f + (kAccentR - 1.f) * mix, 1.f + (kAccentG - 1.f) * mix, 1.f + (kAccentB - 1.f) * mix, a
        );
    }
    ccColor4F const kNoFill = { 0.f, 0.f, 0.f, 0.f };

    float easeOutCubic(float u) {
        float v = 1.f - u;
        return 1.f - v * v * v;
    }

    std::mt19937& rng() {
        static std::mt19937 gen{ std::random_device{}() };
        return gen;
    }
    float roll(float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(rng());
    }
}

MissileNode* MissileNode::create() {
    auto ret = new MissileNode();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool MissileNode::init() {
    if (!CCNode::init()) return false;
    // Origin = the object layer's origin, so every point below is an
    // object-layer coordinate.
    this->setAnchorPoint({ 0.f, 0.f });
    this->setContentSize({ 0.f, 0.f });
    this->setPosition({ 0.f, 0.f });
    this->setID("missile"_spr);

    m_draw = CCDrawNode::create();
    m_draw->setID("draw");
    this->addChild(m_draw);
    return true;
}

void MissileNode::launch(CCPoint from, CCPoint impact, float radius) {
    m_phase = Phase::Falling;
    m_age = 0.f;
    m_from = from;
    m_impact = impact;
    m_radius = radius;
    this->redraw();
}

void MissileNode::detonate() {
    if (m_phase == Phase::Idle) return;
    m_phase = Phase::Blast;
    m_age = 0.f;
    // Evenly spread with jitter, so the spray never comes out lopsided.
    float turn = roll(0.f, 6.2832f);
    for (int i = 0; i < SparkCount; i++) {
        float angle = turn + 6.2832f * static_cast<float>(i) / SparkCount + roll(-0.25f, 0.25f);
        m_sparks[i] = { { std::cos(angle), std::sin(angle) }, m_radius * roll(0.55f, 1.15f), roll(0.38f, 0.55f) };
    }
    this->redraw();
}

void MissileNode::cancel() {
    m_phase = Phase::Idle;
    m_age = 0.f;
    m_draw->clear();
}

void MissileNode::tick(float dt) {
    if (m_phase == Phase::Idle) return;
    m_age += dt;
    if (m_phase == Phase::Blast && m_age >= BlastSeconds) {
        this->cancel();
        return;
    }
    this->redraw();
}

void MissileNode::redraw() {
    m_draw->clear();
    if (m_phase == Phase::Falling) {
        // Reticle: the blast circle, pulsing while the missile is on its way.
        float pulse = 0.5f + 0.5f * std::sin(m_age * 25.f);
        m_draw->drawCircle(m_impact, m_radius, kNoFill, kReticleWidth, premul(1.f, 1.f, 1.f, 0.3f + 0.4f * pulse), kSegments);
        m_draw->drawSegment({ m_impact.x - 6.f, m_impact.y }, { m_impact.x + 6.f, m_impact.y }, 0.8f, premul(1.f, 1.f, 1.f, 0.85f));
        m_draw->drawSegment({ m_impact.x, m_impact.y - 6.f }, { m_impact.x, m_impact.y + 6.f }, 0.8f, premul(1.f, 1.f, 1.f, 0.85f));

        // The missile: eased-in drop from `from` to the impact point, a
        // white body with an orange trail behind it.
        float u = std::clamp(m_age / FallSeconds, 0.f, 1.f);
        float k = u * u;
        CCPoint pos = { m_from.x + (m_impact.x - m_from.x) * k, m_from.y + (m_impact.y - m_from.y) * k };
        float dx = m_impact.x - m_from.x;
        float dy = m_impact.y - m_from.y;
        float len = std::sqrt(dx * dx + dy * dy);
        CCPoint dir = len < 1.f ? CCPoint{ 0.f, -1.f } : CCPoint{ dx / len, dy / len };
        CCPoint tail = { pos.x - dir.x * kBodyLength, pos.y - dir.y * kBodyLength };
        CCPoint trailEnd = { tail.x - dir.x * kTrailLength, tail.y - dir.y * kTrailLength };
        m_draw->drawSegment(tail, trailEnd, kBodyWidth * 1.5f, premul(1.f, 1.f, 1.f, 0.3f));
        m_draw->drawSegment(pos, tail, kBodyWidth, premul(1.f, 1.f, 1.f, 0.95f));
        m_draw->drawCircle(pos, kBodyWidth * 1.3f, premul(1.f, 1.f, 1.f, 1.f), 0.f, kNoFill, 12);
    }
    else if (m_phase == Phase::Blast) {
        this->drawBlast();
    }
}

void MissileNode::drawBlast() {
    float t = m_age;
    float r = m_radius;
    // Filled drawCircle, never drawDot: GD's CCDrawNode draws a dot as a
    // square quad, which is what made the first blast look rectangular
    // (user 2026-09-27).

    // Flash: the cleared circle lights up for an instant as the hazards go.
    if (t < kFlashSeconds) {
        float u = t / kFlashSeconds;
        float fade = (1.f - u) * (1.f - u);
        m_draw->drawCircle(m_impact, r * (0.85f + 0.15f * easeOutCubic(u)), white(0.45f * fade), 0.f, kNoFill, kSegments);
    }

    // Fireball: swells fast, then collapses as it fades; the white core
    // sits on a soft red halo, so a thin red fringe shows round it.
    if (t < kCoreSeconds) {
        float coreR;
        float alpha;
        if (t < kCoreGrowSeconds) {
            coreR = r * kCoreRadius * (0.4f + 0.6f * easeOutCubic(t / kCoreGrowSeconds));
            alpha = 1.f;
        }
        else {
            float v = (t - kCoreGrowSeconds) / (kCoreSeconds - kCoreGrowSeconds);
            coreR = r * kCoreRadius * (1.f - 0.55f * v * v);
            alpha = std::pow(1.f - v, 1.5f);
        }
        m_draw->drawCircle(m_impact, coreR * 1.5f, toAccent(1.f, 0.3f * alpha), 0.f, kNoFill, kSegments);
        m_draw->drawCircle(m_impact, coreR, white(0.95f * alpha), 0.f, kNoFill, kSegments);
    }

    // Shockwave: a thick ring racing past the radius and thinning out, and
    // a fainter echo just behind it.
    if (t < kWaveSeconds) {
        float u = t / kWaveSeconds;
        float waveR = r * (0.3f + (kWaveReach - 0.3f) * easeOutCubic(u));
        m_draw->drawCircle(m_impact, waveR, kNoFill, 0.4f + 2.2f * (1.f - u), white(std::pow(1.f - u, 1.3f)), kSegments);
    }
    if (t > kEchoDelay && t < kEchoDelay + kEchoSeconds) {
        float u = (t - kEchoDelay) / kEchoSeconds;
        float echoR = r * (0.15f + (kEchoReach - 0.15f) * easeOutCubic(u));
        m_draw->drawCircle(m_impact, echoR, kNoFill, 0.3f + 1.f * (1.f - u), white(0.55f * (1.f - u)), kSegments);
    }

    // Sparks: thrown out fast, slowing, pulled down; each drawn as a streak
    // from where it was kSparkLag ago, thinning as it fades and cooling from
    // white to red (by then it is faint, so the red stays a hint).
    auto sparkAt = [&](Spark const& sp, float at) {
        float out = sp.reach * easeOutCubic(std::min(1.f, at / sp.life));
        return CCPoint{ m_impact.x + sp.dir.x * out, m_impact.y + sp.dir.y * out - 0.5f * kSparkGravity * at * at };
    };
    for (auto const& sp : m_sparks) {
        if (t >= sp.life) continue;
        float u = t / sp.life;
        CCPoint head = sparkAt(sp, t);
        CCPoint tail = sparkAt(sp, std::max(0.f, t - kSparkLag));
        // drawSegment normalizes the segment: a zero length is a NaN quad.
        if (std::abs(head.x - tail.x) + std::abs(head.y - tail.y) < 0.5f) continue;
        m_draw->drawSegment(tail, head, 0.3f + 1.1f * (1.f - u), toAccent(u, 0.95f * (1.f - u)));
    }
}

} // namespace augment

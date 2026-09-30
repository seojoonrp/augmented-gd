#include "BerserkAura.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kTau = 6.2831853f;
    constexpr float kPi = 3.14159265f;
    constexpr int kSegments = 20;

    // tongues all the way round; the backward ones are longest so it trails
    // (lengths/offsets are x size)
    constexpr int kTongues = 9;
    constexpr float kBaseRatio = 0.5f;
    constexpr float kTongueArc = 0.30f;   // base half-width, radians
    constexpr float kLenNear = 0.8f;      // tip distance facing forward
    constexpr float kLenFar = 1.8f;       // facing back
    constexpr float kRise = 0.22f;
    constexpr float kFlickRate = 14.f;
    constexpr float kSwirl = 0.8f;        // rad/s
    // inner tongue relative to the outer one
    constexpr float kCoreLen = 0.55f;
    constexpr float kCoreArc = 0.62f;

    constexpr float kGlowRatio = 1.05f;
    constexpr float kGlowAlpha = 0.3f;
    constexpr float kOuterAlpha = 0.5f;
    constexpr float kCoreAlpha = 0.65f;

    // CCDrawNode wants premultiplied alpha.
    ccColor4F premul(float r, float g, float b, float a) {
        return { r * a, g * a, b * a, a };
    }
    ccColor4F const kNoFill = { 0.f, 0.f, 0.f, 0.f };
    ccColor4F ember(float a) { return premul(1.f, 0.30f, 0.06f, a); }
    ccColor4F flame(float a) { return premul(1.f, 0.62f, 0.12f, a); }
    ccColor4F heart(float a) { return premul(1.f, 0.88f, 0.42f, a); }

    CCPoint onCircle(CCPoint centre, float radius, float angle) {
        return { centre.x + std::cos(angle) * radius, centre.y + std::sin(angle) * radius };
    }
}

BerserkAura* BerserkAura::create() {
    auto ret = new BerserkAura();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool BerserkAura::init() {
    if (!CCNode::init()) return false;
    this->setAnchorPoint({ 0.f, 0.f });
    this->setContentSize({ 0.f, 0.f });
    this->setPosition({ 0.f, 0.f });
    this->setID("berserk-aura"_spr);

    m_draw = CCDrawNode::create();
    m_draw->setID("draw");
    this->addChild(m_draw);
    return true;
}

void BerserkAura::tick(float dt, std::span<Flame const> flames, bool on) {
    if (on && !m_on) {
        m_onAge = 0.f;
        m_offAge = -1.f;
    }
    if (!on && m_on) m_offAge = 0.f;
    m_on = on;

    m_age += dt;
    if (m_on) m_onAge += dt;
    else if (m_offAge >= 0.f) {
        m_offAge += dt;
        if (m_offAge >= FadeOutSeconds) m_offAge = -1.f;
    }
    this->redraw(flames);
}

void BerserkAura::reset() {
    m_on = false;
    m_onAge = 0.f;
    m_offAge = -1.f;
    m_draw->clear();
}

void BerserkAura::redraw(std::span<Flame const> flames) {
    m_draw->clear();

    float amp = 0.f;
    if (m_on) amp = std::clamp(m_onAge / FadeInSeconds, 0.f, 1.f);
    else if (m_offAge >= 0.f) amp = 1.f - std::clamp(m_offAge / FadeOutSeconds, 0.f, 1.f);
    if (amp <= 0.f) return;

    for (auto const& f : flames) {
        float size = std::max(1.f, f.size);
        // angle pointing straight behind the player
        float back = f.goingLeft ? 0.f : kPi;

        float breathe = 0.88f + 0.12f * std::sin(m_age * kFlickRate * 0.4f);
        m_draw->drawCircle(f.at, size * kGlowRatio * breathe, ember(kGlowAlpha * amp), 0.f, kNoFill, kSegments);

        for (int i = 0; i < kTongues; i++) {
            float a = static_cast<float>(i) * kTau / kTongues + m_age * kSwirl;
            // 1 = straight back, 0 = straight ahead
            float align = 0.5f + 0.5f * std::cos(a - back);
            float flick = 0.78f + 0.22f * std::sin(m_age * kFlickRate + static_cast<float>(i) * 1.7f);
            float len = size * (kLenNear + (kLenFar - kLenNear) * align) * flick;

            CCPoint baseL = onCircle(f.at, size * kBaseRatio, a - kTongueArc);
            CCPoint baseR = onCircle(f.at, size * kBaseRatio, a + kTongueArc);
            CCPoint tip = onCircle(f.at, len, a);
            tip.y += size * kRise;

            CCPoint outer[3] = { baseL, baseR, tip };
            m_draw->drawPolygon(outer, 3, ember(kOuterAlpha * amp), 0.f, kNoFill);

            CCPoint coreL = onCircle(f.at, size * kBaseRatio * kCoreArc, a - kTongueArc * kCoreArc);
            CCPoint coreR = onCircle(f.at, size * kBaseRatio * kCoreArc, a + kTongueArc * kCoreArc);
            CCPoint coreTip = onCircle(f.at, len * kCoreLen, a);
            coreTip.y += size * kRise * kCoreLen;

            CCPoint core[3] = { coreL, coreR, coreTip };
            m_draw->drawPolygon(core, 3, flame(kCoreAlpha * amp), 0.f, kNoFill);
        }

        m_draw->drawCircle(f.at, size * 0.42f * breathe, heart(0.45f * amp), 0.f, kNoFill, kSegments);
    }
}

} // namespace augment

#include "ShieldNode.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr int kSegments = 40;
    constexpr float kRimWidth = 0.6f;
    constexpr float kFillAlpha = 0.12f;
    constexpr float kRimAlpha = 0.85f;
    constexpr float kBreakGrow = 0.7f;      // burst ring ends at radius * (1 + this)

    // CCDrawNode wants premultiplied alpha.
    ccColor4F white(float a) { return { a, a, a, a }; }
    ccColor4F const kNoFill = { 0.f, 0.f, 0.f, 0.f };
}

ShieldNode* ShieldNode::create() {
    auto ret = new ShieldNode();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ShieldNode::init() {
    if (!CCNode::init()) return false;
    this->setAnchorPoint({ 0.f, 0.f });
    this->setContentSize({ 0.f, 0.f });
    this->setPosition({ 0.f, 0.f });
    this->setID("shield"_spr);

    m_draw = CCDrawNode::create();
    m_draw->setID("draw");
    this->addChild(m_draw);
    return true;
}

void ShieldNode::tick(float dt, std::vector<Bubble> const& bubbles, bool up) {
    if (up && !m_up) m_upAge = 0.f;
    m_up = up;
    if (m_up) m_upAge += dt;
    if (m_breakAge >= 0.f) {
        m_breakAge += dt;
        if (m_breakAge >= BreakSeconds) m_breakAge = -1.f;
    }
    this->redraw(bubbles);
}

void ShieldNode::shatter() {
    m_up = false;
    m_breakAge = 0.f;
}

void ShieldNode::reset() {
    m_breakAge = -1.f;
    m_up = true;
    m_upAge = FadeInSeconds;
    m_draw->clear();
}

void ShieldNode::redraw(std::vector<Bubble> const& bubbles) {
    m_draw->clear();
    for (auto const& b : bubbles) {
        if (m_up) {
            float in = std::clamp(m_upAge / FadeInSeconds, 0.f, 1.f);
            // Pops in slightly oversized, then breathes.
            float pop = 1.f + 0.25f * (1.f - in) * (1.f - in);
            float breathe = 1.f + 0.035f * std::sin(m_upAge * 5.f);
            float r = b.radius * pop * breathe;
            m_draw->drawCircle(b.center, r, white(kFillAlpha * in), kRimWidth, white(kRimAlpha * in), kSegments);
            // Faint inner ring for a bit of depth.
            m_draw->drawCircle(b.center, r * 0.85f, kNoFill, 0.35f, white(0.25f * in), kSegments);
        }
        if (m_breakAge >= 0.f) {
            float t = std::clamp(m_breakAge / BreakSeconds, 0.f, 1.f);
            float ease = 1.f - (1.f - t) * (1.f - t);
            float a = 1.f - t;
            // Just the ring pushed outward: expands, thins and fades.
            m_draw->drawCircle(b.center, b.radius * (1.f + kBreakGrow * ease), white(0.2f * a * a),
                kRimWidth * (1.f + 1.5f * (1.f - t)), white(a), kSegments);
        }
    }
}

} // namespace augment

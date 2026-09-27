#include "MissileNode.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kBodyLength = 22.f;
    constexpr float kBodyWidth = 3.f;
    constexpr float kTrailLength = 40.f;
    constexpr float kReticleWidth = 1.f;
    constexpr int kSegments = 48;

    // CCDrawNode wants premultiplied alpha.
    ccColor4F premul(float r, float g, float b, float a) {
        return { r * a, g * a, b * a, a };
    }
    ccColor4F const kNoFill = { 0.f, 0.f, 0.f, 0.f };
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
        // A filled disc that snaps out to the radius, then a ring that keeps
        // growing a little as both fade.
        float u = std::clamp(m_age / BlastSeconds, 0.f, 1.f);
        float grow = std::min(1.f, u / 0.25f);
        float discR = m_radius * (1.f - (1.f - grow) * (1.f - grow));
        float fade = 1.f - u;
        // All white now (user 2026-09-27), so the depth that the orange used
        // to carry comes from the alphas instead: a soft disc, a bright core,
        // a crisp ring. Filled drawCircle, never drawDot: GD's CCDrawNode
        // draws a dot as a square quad, which is what made the blast look
        // rectangular (user 2026-09-27).
        m_draw->drawCircle(m_impact, discR, premul(1.f, 1.f, 1.f, 0.35f * fade), 0.f, kNoFill, kSegments);
        m_draw->drawCircle(m_impact, discR * 0.45f, premul(1.f, 1.f, 1.f, 0.85f * fade), 0.f, kNoFill, kSegments);
        m_draw->drawCircle(m_impact, m_radius * (1.f + 0.15f * u), kNoFill, 2.f, premul(1.f, 1.f, 1.f, fade), kSegments);
    }
}

} // namespace augment

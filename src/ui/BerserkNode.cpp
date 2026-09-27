#include "BerserkNode.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {
    // The frame is faked as a few nested bands (CCDrawNode has no gradients):
    // kBands rings of kBandWidth each, dimmer toward the middle of the screen.
    constexpr int kBands = 6;
    constexpr float kBandWidth = 9.f;
    constexpr float kFrameAlpha = 0.5f;
    // The frame dims away over the window's last moments.
    constexpr float kFadeOutSeconds = 0.5f;
    constexpr float kPulseRate = 11.f;

    constexpr float kBurstStart = 8.f;
    constexpr float kBurstEnd = 52.f;
    constexpr int kSegments = 28;

    // CCDrawNode wants premultiplied alpha.
    ccColor4F premul(float r, float g, float b, float a) {
        return { r * a, g * a, b * a, a };
    }
    ccColor4F const kNoFill = { 0.f, 0.f, 0.f, 0.f };
}

BerserkNode* BerserkNode::create() {
    auto ret = new BerserkNode();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool BerserkNode::init() {
    if (!CCNode::init()) return false;
    // Origin = the UI layer's origin, so everything below is in screen
    // coordinates.
    this->setAnchorPoint({ 0.f, 0.f });
    this->setContentSize({ 0.f, 0.f });
    this->setPosition({ 0.f, 0.f });
    this->setID("berserk"_spr);

    m_draw = CCDrawNode::create();
    m_draw->setID("draw");
    this->addChild(m_draw);
    return true;
}

void BerserkNode::setWindow(float left, float total) {
    if (left > 0.f && m_left <= 0.f) m_age = 0.f;   // the window just opened
    m_left = std::max(0.f, left);
    m_total = total;
}

void BerserkNode::smashAt(GameObject* obj) {
    if (obj) m_bursts.push_back({ obj, 0.f });
}

void BerserkNode::tick(float dt) {
    if (m_left > 0.f) m_age += dt;
    for (auto& b : m_bursts) b.age += dt;
    std::erase_if(m_bursts, [](Burst const& b) { return !b.target || b.age >= BurstSeconds; });
    if (m_left <= 0.f && m_bursts.empty()) {
        // Nothing to show: clear once, then stay out of the way.
        if (m_dirty) {
            m_draw->clear();
            m_dirty = false;
        }
        return;
    }
    this->redraw();
    m_dirty = true;
}

void BerserkNode::redraw() {
    m_draw->clear();

    if (m_left > 0.f) {
        auto win = CCDirector::get()->getWinSize();
        float fade = std::clamp(m_left / kFadeOutSeconds, 0.f, 1.f);
        // Faster pulse as the window runs out, so "about to end" reads.
        float urgency = m_total > 0.f ? 1.f + (1.f - std::clamp(m_left / m_total, 0.f, 1.f)) : 1.f;
        float pulse = 0.55f + 0.45f * std::sin(m_age * kPulseRate * urgency);
        for (int i = 0; i < kBands; i++) {
            float inner = static_cast<float>(i) * kBandWidth;
            float outer = inner + kBandWidth;
            float drop = 1.f - static_cast<float>(i) / kBands;
            auto col = premul(1.f, 0.15f, 0.1f, kFrameAlpha * drop * drop * pulse * fade);
            // The band as four filled rects (bottom, top, left, right).
            CCPoint bands[4][4] = {
                { { 0.f, inner }, { win.width, inner }, { win.width, outer }, { 0.f, outer } },
                { { 0.f, win.height - outer }, { win.width, win.height - outer }, { win.width, win.height - inner }, { 0.f, win.height - inner } },
                { { inner, outer }, { outer, outer }, { outer, win.height - outer }, { inner, win.height - outer } },
                { { win.width - outer, outer }, { win.width - inner, outer }, { win.width - inner, win.height - outer }, { win.width - outer, win.height - outer } },
            };
            for (auto& quad : bands) m_draw->drawPolygon(quad, 4, col, 0.f, kNoFill);
        }
    }

    for (auto& b : m_bursts) {
        auto obj = b.target.data();
        if (!obj || !obj->getParent()) continue;
        // Object -> screen -> this node (whose origin is the screen's).
        CCPoint world = obj->getParent()->convertToWorldSpace({ obj->getPositionX(), obj->getPositionY() });
        CCPoint at = m_draw->convertToNodeSpace(world);

        float u = std::clamp(b.age / BurstSeconds, 0.f, 1.f);
        float ease = 1.f - (1.f - u) * (1.f - u);
        float radius = kBurstStart + (kBurstEnd - kBurstStart) * ease;
        float alpha = 1.f - u;
        m_draw->drawCircle(at, radius, kNoFill, 2.5f, premul(1.f, 0.25f, 0.15f, alpha), kSegments);
        m_draw->drawDot(at, kBurstStart * (1.f - u), premul(1.f, 0.9f, 0.7f, alpha));
    }
}

} // namespace augment

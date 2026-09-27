#include "CatNode.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kMargin = 14.f;      // from the screen's bottom-right corner
    constexpr float kHalf = 16.f;        // square half-size

    constexpr float kSigilSeconds = 0.55f;
    // Small on purpose (a GD block is ~30 units, so this sits just inside one).
    constexpr float kSigilRadius = 16.f;
    constexpr float kInnerRatio = 0.62f;
    constexpr float kRingWidth = 1.2f;
    constexpr float kSpinRate = 2.2f;    // radians a second
    // Kept deliberately plain (user 2026-09-27: "덜 복잡한"): two rings, four
    // rim ticks and a single triangle. The first version had eight ticks and a
    // six-point hexagram, which read as noise at this size.
    constexpr int kRunes = 4;
    constexpr int kStarPoints = 3;       // 3 points joined in order = one triangle
    constexpr int kStarStep = 1;
    constexpr int kSegments = 24;
    constexpr float kTau = 6.2831853f;

    constexpr ccColor4F kBodyFill = { 1.f, 0.75f, 0.9f, 1.f };   // the CAT notice's pink
    constexpr ccColor4F kBodyBorder = { 0.f, 0.f, 0.f, 1.f };

    // CCDrawNode wants premultiplied alpha.
    ccColor4F premul(float r, float g, float b, float a) {
        return { r * a, g * a, b * a, a };
    }
    ccColor4F const kNoFill = { 0.f, 0.f, 0.f, 0.f };

    CCPoint onCircle(CCPoint centre, float radius, float angle) {
        return { centre.x + std::cos(angle) * radius, centre.y + std::sin(angle) * radius };
    }
}

CatNode* CatNode::create() {
    auto ret = new CatNode();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CatNode::init() {
    if (!CCNode::init()) return false;

    auto winSize = CCDirector::get()->getWinSize();
    // This node's origin is the square's centre; the sigils convert each
    // target's world position into this space, so it does not matter to them.
    this->setAnchorPoint({ 0.f, 0.f });
    this->setContentSize({ 0.f, 0.f });
    this->setPosition({ winSize.width - kMargin - kHalf, kMargin + kHalf });
    this->setID("cat"_spr);

    m_body = CCDrawNode::create();
    CCPoint corners[4] = { { -kHalf, -kHalf }, { kHalf, -kHalf }, { kHalf, kHalf }, { -kHalf, kHalf } };
    m_body->drawPolygon(corners, 4, kBodyFill, 1.f, kBodyBorder);
    m_body->setID("body");
    this->addChild(m_body, 1);

    m_sigils = CCDrawNode::create();
    m_sigils->setID("sigils");
    this->addChild(m_sigils, 0);

    this->scheduleUpdate();
    return true;
}

void CatNode::castAt(std::vector<GameObject*> const& targets) {
    for (auto obj : targets) {
        if (obj) m_active.push_back({ obj, 0.f });
    }
    if (targets.empty()) return;

    // Casting recoil: a quick pop, then settle. Actions run during play (the
    // scheduler is only stopped for drafts).
    m_body->stopAllActions();
    m_body->setScale(1.f);
    m_body->runAction(CCSequence::create(
        CCScaleTo::create(0.05f, 1.3f),
        CCEaseOut::create(CCScaleTo::create(0.2f, 1.f), 2.f),
        nullptr
    ));
    this->redrawSigils();
}

void CatNode::update(float dt) {
    if (m_active.empty()) return;
    for (auto& s : m_active) s.age += dt;
    std::erase_if(m_active, [](Sigil const& s) { return !s.target || s.age >= kSigilSeconds; });
    this->redrawSigils();
}

void CatNode::redrawSigils() {
    m_sigils->clear();
    for (auto& s : m_active) {
        auto obj = s.target.data();
        if (!obj || !obj->getParent()) continue;
        // Object -> screen -> this node (whose origin is the square's centre).
        CCPoint world = obj->getParent()->convertToWorldSpace({ obj->getPositionX(), obj->getPositionY() });
        CCPoint at = m_sigils->convertToNodeSpace(world);

        float u = std::clamp(s.age / kSigilSeconds, 0.f, 1.f);
        // Snaps open over the first quarter, then holds while it fades.
        float grow = std::min(1.f, u / 0.25f);
        float r = kSigilRadius * (1.f - (1.f - grow) * (1.f - grow));
        float fade = 1.f - u * u;
        float spin = s.age * kSpinRate;
        float inner = r * kInnerRatio;
        // White (user 2026-09-27); the pink stays on the corner square only.
        auto ink = [fade](float a) { return premul(1.f, 1.f, 1.f, a * fade); };

        m_sigils->drawCircle(at, r, kNoFill, kRingWidth, ink(0.9f), kSegments);
        m_sigils->drawCircle(at, inner, kNoFill, kRingWidth, ink(0.55f), kSegments);

        // Runes: short ticks in the band between the rings, turning with it.
        for (int i = 0; i < kRunes; i++) {
            float a = spin + static_cast<float>(i) * kTau / kRunes;
            m_sigils->drawSegment(onCircle(at, inner * 1.06f, a), onCircle(at, r * 0.94f, a), 0.9f, ink(0.7f));
        }
        // The star inside, turning the other way so the two read apart.
        for (int i = 0; i < kStarPoints; i++) {
            float a1 = -spin + static_cast<float>(i) * kTau / kStarPoints;
            float a2 = -spin + static_cast<float>(i + kStarStep) * kTau / kStarPoints;
            m_sigils->drawSegment(onCircle(at, inner, a1), onCircle(at, inner, a2), 0.9f, ink(0.8f));
        }
        // A soft core, so the removal still reads against busy level art.
        // Filled drawCircle, not drawDot (which GD draws as a square quad).
        m_sigils->drawCircle(at, inner * 0.35f, ink(0.4f), 0.f, kNoFill, 12);
    }
}

} // namespace augment

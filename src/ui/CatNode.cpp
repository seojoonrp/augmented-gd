#include "CatNode.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kMargin = 14.f;      // from the screen's bottom-right corner
    constexpr float kHalf = 16.f;        // square half-size
    constexpr float kLaserSeconds = 0.45f;
    constexpr float kCoreWidth = 1.2f;
    constexpr float kGlowWidth = 3.5f;

    constexpr ccColor4F kBodyFill = { 1.f, 0.75f, 0.9f, 1.f };   // the CAT notice's pink
    constexpr ccColor4F kBodyBorder = { 0.f, 0.f, 0.f, 1.f };

    // CCDrawNode wants premultiplied alpha.
    ccColor4F premul(float r, float g, float b, float a) {
        return { r * a, g * a, b * a, a };
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
    // This node's origin is the square's centre; the lasers start at (0, 0).
    this->setAnchorPoint({ 0.f, 0.f });
    this->setContentSize({ 0.f, 0.f });
    this->setPosition({ winSize.width - kMargin - kHalf, kMargin + kHalf });
    this->setID("cat"_spr);

    m_body = CCDrawNode::create();
    CCPoint corners[4] = { { -kHalf, -kHalf }, { kHalf, -kHalf }, { kHalf, kHalf }, { -kHalf, kHalf } };
    m_body->drawPolygon(corners, 4, kBodyFill, 1.f, kBodyBorder);
    m_body->setID("body");
    this->addChild(m_body, 1);

    m_lasers = CCDrawNode::create();
    m_lasers->setID("lasers");
    this->addChild(m_lasers, 0);

    this->scheduleUpdate();
    return true;
}

void CatNode::fireAt(std::vector<GameObject*> const& targets) {
    for (auto obj : targets) {
        if (obj) m_active.push_back({ obj, 0.f });
    }
    if (targets.empty()) return;

    // Recoil: a quick pop, then settle. Actions run during play (the
    // scheduler is only stopped for drafts).
    m_body->stopAllActions();
    m_body->setScale(1.f);
    m_body->runAction(CCSequence::create(
        CCScaleTo::create(0.05f, 1.3f),
        CCEaseOut::create(CCScaleTo::create(0.2f, 1.f), 2.f),
        nullptr
    ));
    this->redrawLasers();
}

void CatNode::update(float dt) {
    if (m_active.empty()) return;
    for (auto& l : m_active) l.age += dt;
    std::erase_if(m_active, [](Laser const& l) { return !l.target || l.age >= kLaserSeconds; });
    this->redrawLasers();
}

void CatNode::redrawLasers() {
    m_lasers->clear();
    for (auto& l : m_active) {
        auto obj = l.target.data();
        if (!obj || !obj->getParent()) continue;
        // Object -> screen -> this node (whose origin is the square's centre).
        CCPoint world = obj->getParent()->convertToWorldSpace({ obj->getPositionX(), obj->getPositionY() });
        CCPoint to = m_lasers->convertToNodeSpace(world);

        float t = std::clamp(1.f - l.age / kLaserSeconds, 0.f, 1.f);
        m_lasers->drawSegment({ 0.f, 0.f }, to, kGlowWidth, premul(1.f, 0.4f, 0.8f, 0.45f * t));
        m_lasers->drawSegment({ 0.f, 0.f }, to, kCoreWidth, premul(1.f, 1.f, 1.f, t));
    }
}

} // namespace augment

#include "CatNode.hpp"

#include <algorithm>
#include <cmath>
#include <random>

using namespace geode::prelude;

namespace augment {

namespace {
    // circle frames are 128px uhd (32pt); about one block across on screen
    constexpr float kSigilHold = 0.5f;      // game seconds at full size
    constexpr float kSigilExit = 0.35f;     // spin + shrink + fade
    constexpr float kSigilExitSpin = 200.f; // degrees
    constexpr float kSigilSize = 34.f;      // pt across
    constexpr float kSigilFlip = 0.1f;
    constexpr char const* kSigilSprites[2] = { "cat-magic-1.png"_spr, "cat-magic-2.png"_spr };
    // flash behind a new circle, grows from/to these multiples of kSigilSize
    constexpr float kFlashTime = 0.2f;
    constexpr float kFlashFrom = 1.3f;
    constexpr float kFlashTo = 2.1f;
    constexpr char const* kFlashSprite = "cat-flash.png"_spr;

    float smoothstep(float u) { return u * u * (3.f - 2.f * u); }

    // mascot frames: 256px uhd squares, cat standing on the bottom edge
    constexpr float kMascotSize = 58.f;     // pt tall
    constexpr float kMascotRight = 4.f;     // margins from the screen corner, pt
    constexpr float kMascotBottom = 2.f;
    constexpr float kIdleFrame = 0.5f;
    constexpr float kCastTime = 0.45f;
    constexpr char const* kIdleSprites[2] = { "cat-idle-1.png"_spr, "cat-idle-2.png"_spr };
    constexpr char const* kCastSprite = "cat-cast.png"_spr;
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

    this->setAnchorPoint({ 0.f, 0.f });
    this->setContentSize({ 0.f, 0.f });
    this->setPosition({ 0.f, 0.f });
    this->setID("cat"_spr);

    this->buildMascot();

    this->scheduleUpdate();
    return true;
}

bool CatNode::buildMascot() {
    auto make = [&](char const* file, char const* id) -> CCSprite* {
        auto sprite = CCSprite::create(file);
        if (!sprite) {
            log::warn("Cat: missing sprite {}", file);
            return nullptr;
        }
        sprite->setAnchorPoint({ 1.f, 0.f });
        sprite->setVisible(false);
        sprite->setID(id);
        this->addChild(sprite, 1);
        return sprite;
    };
    m_idle[0] = make(kIdleSprites[0], "cat-idle-1");
    m_idle[1] = make(kIdleSprites[1], "cat-idle-2");
    m_cast = make(kCastSprite, "cat-cast");
    if (!m_idle[0] || !m_idle[1] || !m_cast) {
        for (auto sprite : { m_idle[0], m_idle[1], m_cast }) {
            if (sprite) sprite->removeFromParent();
        }
        m_idle[0] = m_idle[1] = m_cast = nullptr;
        return false;
    }

    auto winSize = CCDirector::get()->getWinSize();
    float const scale = kMascotSize / m_idle[0]->getContentSize().height;
    CCPoint const corner{ winSize.width - kMascotRight, kMascotBottom };
    for (auto sprite : { m_idle[0], m_idle[1], m_cast }) {
        sprite->setScale(scale);
        sprite->setPosition(corner);
    }
    m_shown = m_idle[0];
    m_shown->setVisible(true);
    return true;
}

void CatNode::stepMascot(float dt) {
    if (!m_cast) return;
    m_idleClock = std::fmod(m_idleClock + dt, 2.f * kIdleFrame);
    m_castLeft = std::max(0.f, m_castLeft - dt);

    CCSprite* want = m_castLeft > 0.f ? m_cast : m_idle[m_idleClock < kIdleFrame ? 0 : 1];
    if (want != m_shown) {
        m_shown->setVisible(false);
        want->setVisible(true);
        m_shown = want;
    }
}

void CatNode::castAt(std::vector<GameObject*> const& targets) {
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> turn(0.f, 360.f);
    std::uniform_real_distribution<float> phase(0.f, 2.f * kSigilFlip);
    for (auto obj : targets) {
        if (!obj) continue;
        Sigil sigil;
        sigil.target = obj;
        sigil.phase = phase(rng);
        // same doodle for every circle, a random turn keeps them from matching
        sigil.angle = turn(rng);
        for (int i = 0; i < 2; i++) {
            auto sprite = CCSprite::create(kSigilSprites[i]);
            if (!sprite) break;
            sigil.scale = kSigilSize / sprite->getContentSize().width;
            sprite->setScale(sigil.scale);
            sprite->setRotation(sigil.angle);
            sprite->setVisible(false);
            this->addChild(sprite, 0);
            sigil.frames[i] = sprite;
        }
        if (!sigil.frames[0] || !sigil.frames[1]) {
            for (auto sprite : sigil.frames) {
                if (sprite) sprite->removeFromParent();
            }
            log::warn("Cat: circle sprites missing");
            break;
        }
        // additive like GD's glows (texture is premultiplied, hence ONE/ONE)
        if (auto flash = CCSprite::create(kFlashSprite)) {
            flash->setBlendFunc({ GL_ONE, GL_ONE });
            flash->setRotation(sigil.angle);
            this->addChild(flash, -1);
            sigil.flash = flash;
        }
        m_active.push_back(sigil);
    }
    // nothing in view -> no swing
    if (targets.empty()) return;
    m_castLeft = kCastTime;
    this->stepMascot(0.f);
    this->stepSigils(0.f);
}

void CatNode::update(float dt) {
    this->stepMascot(dt);
    this->stepSigils(dt);
}

void CatNode::stepSigils(float dt) {
    if (m_active.empty()) return;
    auto done = [](Sigil const& sigil) {
        auto obj = sigil.target.data();
        return !obj || !obj->getParent() || sigil.age >= kSigilHold + kSigilExit;
    };
    for (auto& sigil : m_active) {
        sigil.age += dt;
        if (!done(sigil)) continue;
        for (auto sprite : sigil.frames) sprite->removeFromParent();
        if (sigil.flash) sigil.flash->removeFromParent();
    }
    std::erase_if(m_active, done);

    for (auto& sigil : m_active) {
        auto obj = sigil.target.data();
        // object -> screen -> this node
        CCPoint at = this->convertToNodeSpace(obj->getParent()->convertToWorldSpace(obj->getPosition()));

        if (sigil.flash) {
            float const f = sigil.age / kFlashTime;
            bool const lit = f < 1.f;
            sigil.flash->setVisible(lit);
            if (lit) {
                float const grow = 1.f - (1.f - f) * (1.f - f);
                float const across = kSigilSize * (kFlashFrom + (kFlashTo - kFlashFrom) * grow);
                sigil.flash->setPosition(at);
                sigil.flash->setScale(across / sigil.flash->getContentSize().width);
                sigil.flash->setOpacity(static_cast<GLubyte>(255.f * (1.f - f) * (1.f - f)));
            }
        }

        // exit: eased shrink, spin speeding up, fade over the second half.
        // frames stop flipping once it starts
        float const u = std::clamp((sigil.age - kSigilHold) / kSigilExit, 0.f, 1.f);
        float const shrink = smoothstep(u);
        float const fade = 1.f - smoothstep(std::clamp(u * 2.f - 1.f, 0.f, 1.f));
        int const shown = static_cast<int>((std::min(sigil.age, kSigilHold) + sigil.phase) / kSigilFlip) % 2;
        for (int i = 0; i < 2; i++) {
            sigil.frames[i]->setPosition(at);
            sigil.frames[i]->setVisible(i == shown);
            sigil.frames[i]->setScale(sigil.scale * (1.f - shrink));
            sigil.frames[i]->setRotation(sigil.angle + kSigilExitSpin * u * u);
            sigil.frames[i]->setOpacity(static_cast<GLubyte>(255.f * fade));
        }
    }
}

} // namespace augment

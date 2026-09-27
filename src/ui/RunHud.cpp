#include "RunHud.hpp"
#include "Fonts.hpp"

#include <algorithm>
#include <cmath>
#include <random>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kMargin = 6.f;

    // Gauge: GD's own progress-bar sprite, right under the real one.
    constexpr char const* kBarSprite = "GJ_progressBar_001.png";
    constexpr float kFallbackInset = 2.5f;   // when GD's fill can't be measured
    constexpr float kFallbackScale = 0.6f;   // no GD bar to copy: the file is 340x20
    constexpr float kGaugeEase = 8.f;        // per second; ~0.4 s to settle
    constexpr char const* kFullText = "DRAFT!";   // readout while a gauge draft waits

    // Death reward timeline (seconds from the death). Both fills are done
    // by ~0.9 s so they fit GD's respawn delay; settleGauge() cuts whatever
    // is left when the next attempt starts.
    constexpr float kFlyTime = 0.4f;         // one particle's flight
    constexpr float kFlashTime = 0.18f;      // landing burst on the bar
    constexpr float kDepartSpread = 0.12f;   // particles of one colour leave over this
    constexpr float kGoldDelay = 0.15f;      // gold particles / number start after the green ones
    constexpr float kFillTime = 0.25f;       // each segment grows over this
    constexpr float kHoldTime = 0.2f;        // gold stays distinct before it blends in
    constexpr float kMergeTime = 0.3f;
    constexpr float kTextTime = 0.9f;        // a number's lifetime
    constexpr float kTextPop = 0.12f;        // scale-in at the start of it
    constexpr float kTextRise = 16.f;
    constexpr float kTextScale = 0.55f;
    constexpr float kTextGap = 16.f;         // number's distance from the icon centre
    constexpr float kParticleRadius = 3.f;
    // Particles are drawn as filled circles: GD's CCDrawNode::drawDot puts
    // down a square quad instead of a disc (user 2026-09-27).
    constexpr ccColor4F kNoBorder = { 0.f, 0.f, 0.f, 0.f };
    constexpr int kDotSegments = 14;
    constexpr int kMinParticles = 4;
    constexpr int kMaxParticles = 12;
    constexpr float kPi = 3.14159265f;

    // Notices: short Korean lines in the bottom-left corner (left of the
    // gauge, which sits at the bottom centre), newest at the bottom.
    constexpr int kToastLines = 3;
    constexpr float kToastScale = 0.45f;
    constexpr float kToastY = 14.f;      // the bottom line's middle
    constexpr float kToastStep = 14.f;   // an older line slides up by this
    constexpr float kToastSlide = 0.12f;
    constexpr float kToastHold = 1.1f;
    constexpr float kToastFade = 0.45f;

    // Augment column at the far left.
    constexpr float kColumnX = kMargin;
    constexpr float kHeaderScale = 0.36f;
    constexpr float kRowHeight = 15.f;
    constexpr float kRowScale = 0.42f;
    constexpr float kIconBox = 12.f;
    constexpr float kIconGap = 4.f;
    constexpr float kStateGap = 5.f;

    constexpr ccColor3B kFillColor = { 122, 222, 45 };   // the draft cards' GJ_button_01 green
    constexpr ccColor3B kGoldColor = { 255, 210, 60 };   // new-best bonus
    constexpr ccColor3B kStateColor = { 190, 190, 200 };

    std::mt19937& rng() {
        static std::mt19937 gen{ std::random_device{}() };
        return gen;
    }
    float frand(float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(rng());
    }
    float easeOut(float t) {
        t = std::clamp(t, 0.f, 1.f);
        return 1.f - (1.f - t) * (1.f - t);
    }
    ccColor3B lerp(ccColor3B a, ccColor3B b, float t) {
        auto mix = [&](GLubyte x, GLubyte y) { return static_cast<GLubyte>(x + (y - x) * t); };
        return { mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b) };
    }
    ccColor4F opaque(ccColor3B c) {
        return { c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f };
    }
    CCPoint bezier(CCPoint a, CCPoint c, CCPoint b, float t) {
        float u = 1.f - t;
        return a * (u * u) + c * (2.f * u * t) + b * (t * t);
    }
}

RunHud* RunHud::create() {
    auto ret = new RunHud();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool RunHud::init() {
    if (!CCNode::init()) return false;

    auto winSize = CCDirector::get()->getWinSize();
    this->setContentSize(winSize);
    this->setAnchorPoint({ 0.f, 0.f });
    this->setPosition({ 0.f, 0.f });
    this->setID("run-hud"_spr);

    // Placeholder until attachGauge() swaps in GD's percent font.
    m_gaugeLabel = CCLabelBMFont::create("", fonts::Debug);
    m_gaugeLabel->setAnchorPoint({ 0.f, 0.5f });
    m_gaugeLabel->setID("gauge-label");
    this->addChild(m_gaugeLabel);

    m_header = CCLabelBMFont::create("", fonts::Debug);
    m_header->setScale(kHeaderScale);
    m_header->setAnchorPoint({ 0.f, 1.f });
    m_header->setPosition({ kColumnX, winSize.height - kMargin });
    m_header->setColor(kStateColor);
    m_header->setID("header");
    this->addChild(m_header);

    // Notices are player-facing (UI font, white with the baked outline);
    // everything else on this HUD is a debug readout. One label per line,
    // reused in turn — see notice().
    for (int i = 0; i < kToastLines; i++) {
        auto toast = CCLabelBMFont::create("", fonts::Name);
        toast->setScale(kToastScale);
        toast->setAnchorPoint({ 0.f, 0.5f });
        toast->setPosition({ kMargin, kToastY });
        toast->setOpacity(0);
        toast->setID(fmt::format("toast-{}", i));
        this->addChild(toast, 1);
        m_toasts.push_back(toast);
    }

    // Death reward: the numbers beside the dead icon and the particle layer.
    // Player-facing, so the UI font.
    auto makeRewardText = [&](char const* id, ccColor3B color) {
        auto label = CCLabelBMFont::create("", fonts::Name);
        label->setScale(kTextScale);
        label->setColor(color);
        label->setVisible(false);
        label->setID(id);
        this->addChild(label, 2);
        return label;
    };
    m_rewardText.label = makeRewardText("reward-text", kFillColor);
    m_rewardGoldText.label = makeRewardText("reward-gold-text", kGoldColor);
    m_particles = CCDrawNode::create();
    m_particles->setID("reward-particles");
    this->addChild(m_particles, 2);

    this->scheduleUpdate();
    return true;
}

// ---------------------------------------------------------------- gauge

void RunHud::attachGauge(CCSprite* bar, CCSprite* fill, CCLabelBMFont* percentLabel) {
    auto winSize = CCDirector::get()->getWinSize();

    // Same texture as GD's bar (whatever frame it is using), else the
    // standalone file scaled down to roughly the in-level size.
    if (bar && bar->displayFrame()) {
        m_gaugeBar = CCSprite::createWithSpriteFrame(bar->displayFrame());
    }
    if (!m_gaugeBar) m_gaugeBar = CCSprite::create(kBarSprite);
    if (!m_gaugeBar) {
        log::warn("RunHud: no bar sprite, gauge shows as a number only");
        m_gaugeLabel->setPosition({ winSize.width / 2, 16.f });
        return;
    }
    auto size = m_gaugeBar->getContentSize();

    // Copy GD's transform, mirrored to the bottom edge of the screen.
    CCPoint pos;
    if (bar) {
        m_gaugeBar->setScaleX(bar->getScaleX());
        m_gaugeBar->setScaleY(bar->getScaleY());
        m_gaugeBar->setAnchorPoint(bar->getAnchorPoint());
        m_gaugeBar->setColor(bar->getColor());
        m_gaugeBar->setOpacity(bar->getOpacity());
        pos = CCPoint(bar->getPositionX(), winSize.height - bar->getPositionY());
    }
    else {
        m_gaugeBar->setScale(kFallbackScale);
        pos = CCPoint(winSize.width / 2, kMargin + size.height * kFallbackScale / 2);
    }
    m_gaugeBar->setPosition(pos);
    m_gaugeBar->setID("gauge-bar");
    this->addChild(m_gaugeBar);

    // Track = where GD puts its fill inside the same sprite.
    bool fillIsChild = bar && fill && fill->getParent() == bar;
    float inset = fillIsChild ? fill->getPositionX() : kFallbackInset;
    float fillHeight = fillIsChild ? fill->getContentSize().height : size.height - 2.f * inset;
    if (fillHeight <= 0.f || fillHeight > size.height) fillHeight = size.height - 2.f * inset;
    float fillBottom = fillIsChild
        ? fill->getPositionY() - fill->getAnchorPoint().y * fillHeight
        : (size.height - fillHeight) / 2.f;
    m_trackLeft = inset;
    m_trackWidth = std::max(1.f, size.width - 2.f * inset);
    m_fillHeight = fillHeight;
    m_fillBottom = fillBottom;

    m_gaugeFill = CCLayerColor::create({ kFillColor.r, kFillColor.g, kFillColor.b, 255 }, 0.f, fillHeight);
    m_gaugeFill->setPosition({ m_trackLeft, fillBottom });
    m_gaugeFill->setID("gauge-fill");
    m_gaugeBar->addChild(m_gaugeFill, -1);
    // The new-best segment sits right after the green one (layoutFill).
    m_gaugeGold = CCLayerColor::create({ kGoldColor.r, kGoldColor.g, kGoldColor.b, 255 }, 0.f, fillHeight);
    m_gaugeGold->setPosition({ m_trackLeft, fillBottom });
    m_gaugeGold->setID("gauge-gold");
    m_gaugeBar->addChild(m_gaugeGold, -1);

    // Readout: GD's percent label, mirrored the same way.
    if (percentLabel) {
        auto twin = CCLabelBMFont::create("", percentLabel->getFntFile());
        if (twin) {
            m_gaugeLabel->removeFromParent();
            m_gaugeLabel = twin;
            this->addChild(m_gaugeLabel);
        }
        m_gaugeLabel->setScale(percentLabel->getScale());
        m_gaugeLabel->setAnchorPoint(percentLabel->getAnchorPoint());
        m_gaugeLabel->setPosition({ percentLabel->getPositionX(), winSize.height - percentLabel->getPositionY() });
    }
    else {
        auto twin = CCLabelBMFont::create("", "bigFont.fnt");
        if (twin) {
            m_gaugeLabel->removeFromParent();
            m_gaugeLabel = twin;
            this->addChild(m_gaugeLabel);
        }
        m_gaugeLabel->setScale(0.5f);
        m_gaugeLabel->setAnchorPoint({ 0.f, 0.5f });
        float anchorX = m_gaugeBar->getAnchorPoint().x;
        m_gaugeLabel->setPosition({ pos.x + m_gaugeBar->getScaledContentSize().width * (1.f - anchorX) + 6.f, pos.y });
    }
    m_gaugeLabel->setID("gauge-label");

    log::info(
        "RunHud gauge: bar {}x{} scale ({:.2f}, {:.2f}) at ({:.1f}, {:.1f}); GD fill {} -> inset {:.1f} track {:.1f} fill h {:.1f} bottom {:.1f}; label {} scale {:.2f} at ({:.1f}, {:.1f})",
        size.width, size.height, m_gaugeBar->getScaleX(), m_gaugeBar->getScaleY(), pos.x, pos.y,
        fillIsChild ? "measured" : "guessed", inset, m_trackWidth, fillHeight, fillBottom,
        percentLabel ? "copied" : "fallback", m_gaugeLabel->getScale(), m_gaugeLabel->getPositionX(), m_gaugeLabel->getPositionY()
    );
}

void RunHud::setGauge(float value, float threshold, bool full) {
    m_gaugeFull = full;
    m_gaugeTarget = full ? 1.f : threshold > 0.f ? std::clamp(value / threshold, 0.f, 1.f) : 0.f;
    m_gaugeText = full ? kFullText : fmt::format("{:.0f}/{:.0f}", std::min(value, threshold), threshold);
    // During a reward the readout counts up with the fill instead.
    if (!m_reward.active && m_gaugeText != m_gaugeLabel->getString()) m_gaugeLabel->setString(m_gaugeText.c_str());
}

void RunHud::layoutFill() {
    if (!m_gaugeFill) return;
    float green = std::round(m_trackWidth * m_gaugeShown);
    float goldEnd = std::round(m_trackWidth * std::max(m_goldShown, m_gaugeShown));
    m_gaugeFill->setContentSize({ green, m_fillHeight });
    m_gaugeGold->setPosition({ m_trackLeft + green, m_fillBottom });
    m_gaugeGold->setContentSize({ std::max(0.f, goldEnd - green), m_fillHeight });
}

CCPoint RunHud::tipAt(float ratio) {
    CCPoint inBar{ m_trackLeft + m_trackWidth * std::clamp(ratio, 0.f, 1.f), m_fillBottom + m_fillHeight / 2.f };
    return this->convertToNodeSpace(m_gaugeBar->convertToWorldSpace(inBar));
}

void RunHud::update(float dt) {
    if (m_reward.active) {
        this->stepReward(dt);
        return;
    }
    if (!m_gaugeFill) return;
    float diff = m_gaugeTarget - m_gaugeShown;
    if (std::fabs(diff) < 0.002f) {
        if (diff == 0.f) return;
        m_gaugeShown = m_gaugeTarget;
    }
    else {
        m_gaugeShown += diff * std::min(1.f, dt * kGaugeEase);
    }
    m_goldShown = m_gaugeShown;
    this->layoutFill();
}

// ---------------------------------------------------------------- death reward

void RunHud::playDeathReward(CCPoint at, float before, float normal, float bonus, float threshold) {
    if (threshold <= 0.f || normal + bonus <= 0.f) return;
    if (m_reward.active) this->finishReward(m_gaugeTarget);
    bool gold = bonus > 0.f;

    auto& r = m_reward;
    r = Reward{};
    r.active = true;
    r.start = std::clamp(before / threshold, 0.f, 1.f);
    r.mid = std::clamp((before + normal) / threshold, 0.f, 1.f);
    r.end = std::clamp((before + normal + bonus) / threshold, 0.f, 1.f);
    // Each fill starts as the bulk of its particles lands.
    r.fillAt = kFlyTime + kDepartSpread / 2.f;
    r.goldFillAt = r.fillAt + kFillTime;
    r.mergeAt = r.goldFillAt + (gold ? kFillTime + kHoldTime : 0.f);
    float lastText = (gold ? kGoldDelay : 0.f) + kTextTime;
    r.endAt = std::max(r.mergeAt + (gold ? kMergeTime : 0.f), lastText);
    r.threshold = threshold;
    m_gaugeShown = r.start;
    m_goldShown = r.start;
    this->layoutFill();

    this->placeRewardText(m_rewardText, fmt::format("+{:.0f}", normal), at, 9.f, 0.f);
    if (gold) this->placeRewardText(m_rewardGoldText, fmt::format("+{:.0f}", bonus), at, -8.f, kGoldDelay);
    else {
        m_rewardGoldText.used = false;
        m_rewardGoldText.label->setVisible(false);
    }

    int nNormal = 0, nGold = 0;
    if (m_gaugeFill) {
        // More charge, more particles; the gold ones leave later and aim
        // where the bar will end after the gold fill.
        nNormal = std::clamp(kMinParticles + static_cast<int>(normal / 8.f), kMinParticles, kMaxParticles);
        this->spawnParticles(nNormal, false, at, this->tipAt(r.mid), 0.f);
        if (gold) {
            nGold = std::clamp(kMinParticles - 1 + static_cast<int>(bonus / 6.f), kMinParticles - 1, kMaxParticles);
            this->spawnParticles(nGold, true, at, this->tipAt(r.end), kGoldDelay);
        }
    }
    CCPoint tip = m_gaugeFill ? this->tipAt(r.mid) : CCPoint{};
    log::info(
        "RunHud reward: +{:.0f} (+{:.0f} gold) at ({:.0f}, {:.0f}) -> tip ({:.0f}, {:.0f}); fill {:.2f} -> {:.2f} -> {:.2f}; {} + {} particles, ends at {:.2f} s",
        normal, bonus, at.x, at.y, tip.x, tip.y, r.start, r.mid, r.end, nNormal, nGold, r.endAt
    );
}

void RunHud::spawnParticles(int count, bool gold, CCPoint from, CCPoint to, float departAfter) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.gold = gold;
        p.from = from + CCPoint(frand(-5.f, 5.f), frand(-5.f, 5.f));
        p.to = to + CCPoint(frand(-2.f, 2.f), frand(-1.f, 1.f));
        // Arc: control point off the straight line, biased upward so the
        // swarm lifts off the icon before it dives into the bar.
        CCPoint d = p.to - p.from;
        float len = std::max(1.f, d.getLength());
        CCPoint side{ -d.y / len, d.x / len };
        p.ctrl = (p.from + p.to) * 0.5f + side * (frand(-1.f, 1.f) * frand(40.f, 90.f)) + CCPoint(0.f, frand(10.f, 40.f));
        p.depart = departAfter + frand(0.f, kDepartSpread);
        m_reward.particles.push_back(p);
    }
}

void RunHud::placeRewardText(RewardText& text, std::string const& str, CCPoint at, float dy, float born) {
    auto winSize = CCDirector::get()->getWinSize();
    auto label = text.label;
    label->setString(str.c_str());
    label->setScale(kTextScale);
    float width = label->getContentSize().width * kTextScale;
    // Right of the icon unless that runs off screen, then left of it.
    bool right = at.x + kTextGap + width <= winSize.width - kMargin;
    label->setAnchorPoint({ right ? 0.f : 1.f, 0.5f });
    text.baseY = std::clamp(at.y + dy, kMargin + 8.f, winSize.height - kMargin - 8.f - kTextRise);
    label->setPosition({ right ? at.x + kTextGap : at.x - kTextGap, text.baseY });
    text.used = true;
    text.born = born;
    label->setOpacity(0);
    label->setVisible(false);
}

void RunHud::animateRewardText(RewardText& text) {
    float age = m_reward.t - text.born;
    auto label = text.label;
    if (!text.used || age < 0.f || age >= kTextTime) {
        label->setVisible(false);
        return;
    }
    label->setVisible(true);
    // Pop in with a little overshoot, drift up, fade over the last third.
    float pop = std::min(1.f, age / kTextPop);
    float life = age / kTextTime;
    label->setScale(kTextScale * (1.f + 0.35f * std::sin(kPi * pop)));
    label->setPositionY(text.baseY + kTextRise * life);
    label->setOpacity(static_cast<GLubyte>(255.f * std::min(1.f, (1.f - life) / 0.3f)));
}

void RunHud::stepReward(float dt) {
    auto& r = m_reward;
    r.t += dt;

    this->animateRewardText(m_rewardText);
    this->animateRewardText(m_rewardGoldText);

    m_particles->clear();
    for (auto& p : r.particles) {
        float u = (r.t - p.depart) / kFlyTime;
        if (u < 0.f) continue;
        if (u >= 1.f) {
            if (!p.landed) {
                p.landed = true;
                r.flashes.push_back({ p.to, r.t, p.gold });
            }
            continue;
        }
        // Quick lift-off, then a steady dive into the bar (ease-in-out, so
        // the approach stays visible); shrinking on the way.
        float e = u < 0.5f ? 2.f * u * u : 1.f - 2.f * (1.f - u) * (1.f - u);
        CCPoint pos = bezier(p.from, p.ctrl, p.to, e);
        float radius = kParticleRadius * (1.f - 0.3f * u);
        m_particles->drawCircle(pos, radius + 1.f, { 0.f, 0.f, 0.f, 1.f }, 0.f, kNoBorder, kDotSegments);
        m_particles->drawCircle(pos, radius, opaque(p.gold ? kGoldColor : kFillColor), 0.f, kNoBorder, kDotSegments);
    }
    // Landing: a ring that grows and thins out where the particle hit.
    std::erase_if(r.flashes, [&](Reward::Flash const& f) { return r.t - f.born >= kFlashTime; });
    for (auto const& f : r.flashes) {
        float k = (r.t - f.born) / kFlashTime;
        auto c = f.gold ? kGoldColor : kFillColor;
        float a = 1.f - k;
        m_particles->drawCircle(f.at, kParticleRadius * (1.f + 2.f * k), { c.r / 255.f * a, c.g / 255.f * a, c.b / 255.f * a, a }, 0.f, kNoBorder, kDotSegments);
    }

    if (m_gaugeFill) {
        m_gaugeShown = r.start + (r.mid - r.start) * easeOut((r.t - r.fillAt) / kFillTime);
        m_goldShown = m_gaugeShown + (r.end - r.mid) * easeOut((r.t - r.goldFillAt) / kFillTime);
        if (r.t >= r.mergeAt) m_gaugeGold->setColor(lerp(kGoldColor, kFillColor, (r.t - r.mergeAt) / kMergeTime));
        this->layoutFill();
        if (r.t >= r.fillAt) {
            // Count up against the cost this death was filling toward; once
            // the bar is full a waiting draft shows as DRAFT! right away.
            std::string text = m_gaugeFull && m_goldShown >= 0.999f
                ? kFullText
                : fmt::format("{:.0f}/{:.0f}", std::round(m_goldShown * r.threshold), r.threshold);
            if (text != m_gaugeLabel->getString()) m_gaugeLabel->setString(text.c_str());
        }
    }

    if (r.t >= r.endAt) this->finishReward(r.end);
}

void RunHud::finishReward(float ratio) {
    m_reward.active = false;
    m_reward.particles.clear();
    m_reward.flashes.clear();
    m_particles->clear();
    m_rewardText.label->setVisible(false);
    m_rewardGoldText.label->setVisible(false);
    m_gaugeShown = ratio;
    m_goldShown = ratio;
    if (m_gaugeGold) m_gaugeGold->setColor(kGoldColor);
    this->layoutFill();
    if (m_gaugeText != m_gaugeLabel->getString()) m_gaugeLabel->setString(m_gaugeText.c_str());
}

void RunHud::settleGauge() {
    if (!m_reward.active) return;
    log::info("RunHud reward: cut at {:.2f} s (attempt start)", m_reward.t);
    this->finishReward(m_gaugeTarget);
}

// ---------------------------------------------------------------- column

void RunHud::setHeader(std::string const& text) {
    if (text != m_header->getString()) m_header->setString(text.c_str());
}

RunHud::SlotNodes RunHud::makeSlot(size_t index) {
    auto winSize = CCDirector::get()->getWinSize();
    float rowTop = winSize.height - kMargin - kRowHeight * static_cast<float>(index + 1);

    SlotNodes s;
    s.root = CCNode::create();
    s.root->setPosition({ kColumnX, rowTop });
    s.root->setID(fmt::format("slot-{}", index));
    this->addChild(s.root);

    // Placeholder for the augment icon: a framed empty box.
    auto frame = CCLayerColor::create({ 255, 255, 255, 110 }, kIconBox, kIconBox);
    frame->setPosition({ 0.f, -kIconBox });
    s.root->addChild(frame);
    auto inner = CCLayerColor::create({ 0, 0, 0, 150 }, kIconBox - 2.f, kIconBox - 2.f);
    inner->setPosition({ 1.f, -kIconBox + 1.f });
    s.root->addChild(inner);

    // Text is centred on the box.
    float textY = -kIconBox / 2.f;
    s.name = CCLabelBMFont::create("", fonts::Debug);
    s.name->setScale(kRowScale);
    s.name->setAnchorPoint({ 0.f, 0.5f });
    s.name->setPosition({ kIconBox + kIconGap, textY });
    s.root->addChild(s.name);

    s.state = CCLabelBMFont::create("", fonts::Debug);
    s.state->setScale(kRowScale);
    s.state->setAnchorPoint({ 0.f, 0.5f });
    s.state->setPositionY(textY);
    s.state->setColor(kStateColor);
    s.root->addChild(s.state);
    return s;
}

void RunHud::setSlots(std::vector<Slot> const& slots) {
    while (m_slots.size() < slots.size()) m_slots.push_back(this->makeSlot(m_slots.size()));

    for (size_t i = 0; i < m_slots.size(); i++) {
        auto& s = m_slots[i];
        if (i >= slots.size()) {
            s.root->setVisible(false);
            continue;
        }
        s.root->setVisible(true);
        bool moved = false;
        if (slots[i].name != s.name->getString()) {
            s.name->setString(slots[i].name.c_str());
            moved = true;
        }
        if (slots[i].state != s.state->getString()) s.state->setString(slots[i].state.c_str());
        if (moved) {
            float nameRight = s.name->getPositionX() + s.name->getContentSize().width * kRowScale;
            s.state->setPositionX(nameRight + kStateGap);
        }
    }
}

// ---------------------------------------------------------------- notice

void RunHud::notice(std::string const& text) {
    // The label taken next is the one that has been on screen longest, so
    // whatever is still visible slides up a line and the new text always
    // appears in the corner. A label at opacity 0 has finished fading.
    auto label = m_toasts[m_nextToast];
    m_nextToast = (m_nextToast + 1) % m_toasts.size();
    for (auto other : m_toasts) {
        if (other == label || other->getOpacity() == 0) continue;
        other->runAction(CCMoveBy::create(kToastSlide, { 0.f, kToastStep }));
    }

    label->stopAllActions();
    label->setString(text.c_str());
    label->setPosition({ kMargin, kToastY });
    label->setOpacity(255);
    label->runAction(CCSequence::create(
        CCDelayTime::create(kToastHold),
        CCFadeOut::create(kToastFade),
        nullptr
    ));
}

} // namespace augment

#include "RunHud.hpp"
#include "CardStyle.hpp"
#include "Fonts.hpp"
#include "../game/Scales.hpp"

#include <Geode/loader/SettingV3.hpp>

#include <algorithm>
#include <cmath>
#include <random>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kMargin = 6.f;

    // gauge bar. 240 wide keeps it clear of the longest corner notice (ends at x ~156)
    constexpr float kBarWidth = 240.f;
    constexpr float kBarHeight = 14.f;
    constexpr float kBarRadius = 5.f;
    // white outside the black ring, a black rim alone vanished on dark levels
    constexpr float kBarWhite = 1.25f;
    constexpr float kBarBlack = 1.f;
    constexpr float kBarRim = kBarWhite + kBarBlack;
    constexpr float kTrackAlpha = 0.47f;
    constexpr float kFillInset = 1.f;
    constexpr float kFillRadius = kBarRadius - kBarRim - kFillInset;   // concentric with the bar
    constexpr float kBarBottom = 7.f;        // screen bottom -> bar
    constexpr float kLabelGap = 2.f;
    constexpr float kLabelShrink = 0.9f;     // of GD's percent label scale
    constexpr float kFallbackLabelScale = 0.5f;
    constexpr char const* kDraftText = "DRAFT";
    constexpr char const* kMaxText = "MAX";
    constexpr float kGaugeEase = 8.f;        // per second

    // Death reward, in seconds from the death. GD respawns ~1 s after death and
    // settleGauge() cuts whatever's left then, so the whole thing has to fit.
    constexpr float kRespawnBudget = 0.95f;
    constexpr float kFlyTime = 0.35f;
    constexpr float kFlashTime = 0.18f;
    constexpr float kDepartSpread = 0.12f;
    constexpr float kGoldDelay = 0.15f;
    constexpr float kFillTime = 0.2f;
    constexpr float kHoldTime = 0.05f;       // gold stays distinct before blending in
    constexpr float kMergeTime = 0.14f;
    constexpr float kTextTime = 0.8f;
    // fills start as most of their particles land
    constexpr float kFillAt = kFlyTime + kDepartSpread / 2.f;
    constexpr float kGoldFillAt = kFillAt + kGoldDelay;
    static_assert(kGoldFillAt + kFillTime + kHoldTime + kMergeTime <= kRespawnBudget + 1e-4f,
        "the gold merge must finish before GD respawns");
    static_assert(kGoldDelay + kTextTime <= kRespawnBudget + 1e-4f, "the +bonus number must fade out before GD respawns");
    constexpr float kTextPop = 0.12f;
    constexpr float kTextRise = 16.f;
    constexpr float kTextScale = 0.55f;
    constexpr float kTextGap = 16.f;
    constexpr float kParticleRadius = 3.f;
    // particles go through drawCircle, drawDot draws a square
    constexpr ccColor4F kNoBorder = { 0.f, 0.f, 0.f, 0.f };
    constexpr int kDotSegments = 14;
    constexpr int kMinParticles = 4;
    constexpr int kMaxParticles = 12;
    constexpr float kPi = 3.14159265f;

    // notices
    constexpr int kToastLines = 3;
    constexpr float kToastScale = 0.45f;
    constexpr float kToastX = 12.f;
    constexpr float kToastY = 20.f;      // bottom line's middle
    constexpr float kToastStep = 14.f;
    constexpr float kToastRise = 10.f;   // starts/ends this far below its line
    constexpr float kToastIn = 0.32f;
    constexpr float kToastHold = 1.1f;
    constexpr float kToastOut = 0.38f;
    constexpr float kToastStackRate = 14.f;   // per second

    // berserk banner
    constexpr float kBannerScale = 0.7f;
    constexpr ccColor3B kBannerColor = { 255, 64, 48 };
    constexpr float kBannerGap = 6.f;
    constexpr float kBannerRise = 14.f;
    constexpr float kBannerIn = 0.3f;
    constexpr float kBannerHold = 0.9f;
    constexpr float kBannerOut = 0.4f;

    // debug column
    constexpr float kColumnX = kMargin;
    constexpr float kHeaderScale = 0.36f;
    constexpr float kRowHeight = 15.f;
    constexpr float kRowScale = 0.42f;
    constexpr float kIconBox = 12.f;
    constexpr float kIconGap = 4.f;
    constexpr float kStateGap = 5.f;

    constexpr ccColor3B kFillColor = { 122, 222, 45 };   // GJ_button_01 green, same as the cards
    constexpr ccColor3B kGoldColor = { 255, 210, 60 };
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
    // lift: 0 -> 1 on the way in (ease-out cubic), back to 0 on the way out
    // (ease-in cubic). False once it's over.
    bool envelope(float age, float in, float hold, float out, float& lift) {
        if (age < in) {
            float const u = 1.f - age / in;
            lift = 1.f - u * u * u;
            return true;
        }
        if (age < in + hold) {
            lift = 1.f;
            return true;
        }
        if (age < in + hold + out) {
            float const u = (age - in - hold) / out;
            lift = 1.f - u * u * u;
            return true;
        }
        lift = 0.f;
        return false;
    }
    // scheduler dt is scaled by slow-mo and the brake, the UI shouldn't be
    float realDt(float dt) {
        return dt / std::max(0.05f, scales::time());
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
    constexpr char const* kOpacitySetting = "draft-bar-opacity";   // percent, 5..100
    float opacityOf(int64_t percent) {
        return std::clamp(static_cast<float>(percent) / 100.f, 0.f, 1.f);
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

    // placeholder, attachGauge() swaps in GD's percent font
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

    for (int i = 0; i < kToastLines; i++) {
        auto label = CCLabelBMFont::create("", fonts::Name);
        label->setScale(kToastScale);
        label->setAnchorPoint({ 0.f, 0.5f });
        label->setPosition({ kToastX, kToastY });
        label->setOpacity(0);
        label->setID(fmt::format("toast-{}", i));
        this->addChild(label, 1);
        Toast toast;
        toast.label = label;
        m_toasts.push_back(toast);
    }
    m_banner.label = CCLabelBMFont::create("", fonts::Name);
    m_banner.label->setScale(kBannerScale);
    m_banner.label->setColor(kBannerColor);
    m_banner.label->setOpacity(0);
    m_banner.label->setID("banner");
    this->addChild(m_banner.label, 1);

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

    // live, since the settings page can be opened over a paused level
    m_barOpacity = opacityOf(Mod::get()->getSettingValue<int64_t>(kOpacitySetting));
    this->addEventListener(
        SettingChangedEventV3(Mod::get(), kOpacitySetting),
        [this](std::shared_ptr<SettingV3> setting) {
            auto value = cast::typeinfo_pointer_cast<IntSettingV3>(setting);
            if (!value) return;
            this->setBarOpacity(opacityOf(value->getValue()));
        }
    );

    this->scheduleUpdate();
    return true;
}

// ---------------------------------------------------------------- gauge

void RunHud::attachGauge(CCLabelBMFont* percentLabel) {
    auto winSize = CCDirector::get()->getWinSize();

    auto bar = CCNode::create();
    bar->setContentSize({ kBarWidth, kBarHeight });
    bar->setAnchorPoint({ 0.5f, 0.5f });
    bar->setPosition({ winSize.width / 2, kBarBottom + kBarHeight / 2 });
    bar->setID("gauge-bar");
    this->addChild(bar);
    m_gaugeBar = bar;

    m_gaugeFrame = CCDrawNode::create();
    m_gaugeFrame->setID("gauge-frame");
    bar->addChild(m_gaugeFrame, 0);
    this->drawFrame();

    m_trackLeft = kBarRim + kFillInset;
    m_trackWidth = kBarWidth - 2 * m_trackLeft;
    m_fillBottom = kBarRim + kFillInset;
    m_fillHeight = kBarHeight - 2 * m_fillBottom;
    m_gaugeFill = CCDrawNode::create();
    m_gaugeFill->setID("gauge-fill");
    bar->addChild(m_gaugeFill, 1);
    this->layoutFill();

    char const* font = percentLabel ? percentLabel->getFntFile() : "bigFont.fnt";
    float const scale = (percentLabel ? percentLabel->getScale() : kFallbackLabelScale) * kLabelShrink;
    float const left = winSize.width / 2 - kBarWidth / 2;
    float const right = winSize.width / 2 + kBarWidth / 2;
    float const labelY = kBarBottom + kBarHeight + kLabelGap;
    if (auto count = CCLabelBMFont::create(m_gaugeText.c_str(), font)) {
        m_gaugeLabel->removeFromParent();
        m_gaugeLabel = count;
        this->addChild(m_gaugeLabel);
    }
    m_gaugeLabel->setScale(scale);
    m_gaugeLabel->setAnchorPoint({ 1.f, 0.f });
    m_gaugeLabel->setPosition({ right, labelY });
    m_gaugeLabel->setID("gauge-label");
    m_draftLabel = CCLabelBMFont::create(m_gaugeMax ? kMaxText : kDraftText, font);
    if (m_draftLabel) {
        m_draftLabel->setScale(scale);
        m_draftLabel->setAnchorPoint({ 0.f, 0.f });
        m_draftLabel->setPosition({ left, labelY });
        m_draftLabel->setID("gauge-title");
        this->addChild(m_draftLabel);
    }
    m_gaugeTop = labelY + (m_draftLabel ? m_draftLabel->getScaledContentSize().height : 0.f);
    m_gaugeLabel->setOpacity(this->barAlpha());
    if (m_draftLabel) m_draftLabel->setOpacity(this->barAlpha());
}

void RunHud::setBarOpacity(float opacity) {
    opacity = std::clamp(opacity, 0.f, 1.f);
    if (opacity == m_barOpacity) return;
    m_barOpacity = opacity;
    m_gaugeLabel->setOpacity(this->barAlpha());
    if (m_draftLabel) m_draftLabel->setOpacity(this->barAlpha());
    this->drawFrame();
    m_drawnGreen = -1.f;   // fill is baked in the old alpha, force a redraw
    this->layoutFill();
}

GLubyte RunHud::barAlpha() const {
    return static_cast<GLubyte>(std::lround(255.f * m_barOpacity));
}

// White ring, then the black ring and the see-through track as one polygon.
// Borders are centred on the path, hence the half-width inset.
void RunHud::drawFrame() {
    if (!m_gaugeFrame) return;
    m_gaugeFrame->clear();
    float const a = m_barOpacity;
    auto ring = [&](float inset, float width, ccColor4F fill, ccColor4F border) {
        float const at = inset + width / 2;
        auto path = card::roundedRectPoints(
            { at, at, kBarWidth - 2 * at, kBarHeight - 2 * at }, kBarRadius - at
        );
        m_gaugeFrame->drawPolygon(path.data(), static_cast<unsigned int>(path.size()), fill, width / 2, border);
    };
    ring(0.f, kBarWhite, card::premul({ 0, 0, 0 }, 0.f), card::premul({ 255, 255, 255 }, a));
    ring(kBarWhite, kBarBlack, card::premul({ 0, 0, 0 }, kTrackAlpha * a), card::premul({ 0, 0, 0 }, a));
}

// setString can put reused glyphs back at full opacity, so reapply ours
void RunHud::setBarText(CCLabelBMFont* label, std::string const& text) {
    if (!label || text == label->getString()) return;
    label->setString(text.c_str());
    label->setOpacity(this->barAlpha());
}

void RunHud::showGaugeText(std::string const& text) {
    this->setBarText(m_gaugeLabel, text);
}

void RunHud::setGauge(float value, float threshold, bool full, bool max) {
    // called at 10 Hz, usually with nothing new
    if (value == m_gaugeValue && threshold == m_gaugeCost && full == m_gaugeFull && max == m_gaugeMax) return;
    m_gaugeValue = value;
    m_gaugeCost = threshold;
    m_gaugeFull = full;
    m_gaugeMax = max;
    m_gaugeTarget = full || max ? 1.f : threshold > 0.f ? std::clamp(value / threshold, 0.f, 1.f) : 0.f;
    m_gaugeText = max ? ""
        : full ? fmt::format("{0:.0f}/{0:.0f}", threshold)
        : fmt::format("{:.0f}/{:.0f}", std::min(value, threshold), threshold);
    this->setBarText(m_draftLabel, max ? kMaxText : kDraftText);
    // mid-reward the count ticks up with the fill instead
    if (!m_reward.active) this->showGaugeText(m_gaugeText);
}

// gold under green; roundedRectPoints clamps the radius on narrow segments
void RunHud::layoutFill() {
    if (!m_gaugeFill) return;
    float const green = m_trackWidth * std::clamp(m_gaugeShown, 0.f, 1.f);
    float const gold = m_trackWidth * std::clamp(std::max(m_goldShown, m_gaugeShown), 0.f, 1.f);
    bool const sameTint = m_goldTint.r == m_drawnTint.r && m_goldTint.g == m_drawnTint.g && m_goldTint.b == m_drawnTint.b;
    if (std::fabs(green - m_drawnGreen) < 0.05f && std::fabs(gold - m_drawnGold) < 0.05f && sameTint) return;
    m_drawnGreen = green;
    m_drawnGold = gold;
    m_drawnTint = m_goldTint;

    m_gaugeFill->clear();
    auto segment = [&](float width, ccColor3B colour) {
        if (width < 0.2f) return;
        auto points = card::roundedRectPoints({ m_trackLeft, m_fillBottom, width, m_fillHeight }, kFillRadius);
        m_gaugeFill->drawPolygon(
            points.data(), static_cast<unsigned int>(points.size()),
            card::premul(colour, m_barOpacity), 0.f, card::premul({ 0, 0, 0 }, 0.f)
        );
    };
    if (gold > green + 0.05f) segment(gold, m_goldTint);
    segment(green, kFillColor);
}

CCPoint RunHud::tipAt(float ratio) {
    CCPoint inBar{ m_trackLeft + m_trackWidth * std::clamp(ratio, 0.f, 1.f), m_fillBottom + m_fillHeight / 2.f };
    return this->convertToNodeSpace(m_gaugeBar->convertToWorldSpace(inBar));
}

void RunHud::update(float dt) {
    float const real = realDt(dt);
    this->stepToasts(real);
    this->stepBanner(real);
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
    r.fillAt = kFillAt;
    r.goldFillAt = kGoldFillAt;
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

    if (m_gaugeFill) {
        // gold ones leave later and aim at the end of the gold fill
        int const nNormal = std::clamp(kMinParticles + static_cast<int>(normal / 8.f), kMinParticles, kMaxParticles);
        this->spawnParticles(nNormal, false, at, this->tipAt(r.mid), 0.f);
        if (gold) {
            int const nGold = std::clamp(kMinParticles - 1 + static_cast<int>(bonus / 6.f), kMinParticles - 1, kMaxParticles);
            this->spawnParticles(nGold, true, at, this->tipAt(r.end), kGoldDelay);
        }
    }
}

void RunHud::spawnParticles(int count, bool gold, CCPoint from, CCPoint to, float departAfter) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.gold = gold;
        p.from = from + CCPoint(frand(-5.f, 5.f), frand(-5.f, 5.f));
        p.to = to + CCPoint(frand(-2.f, 2.f), frand(-1.f, 1.f));
        // control point off to the side and biased up, so the swarm lifts off before diving in
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
    // pop with a bit of overshoot, drift up, fade out over the last 30%
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
        // ease-in-out so the approach stays visible
        float e = u < 0.5f ? 2.f * u * u : 1.f - 2.f * (1.f - u) * (1.f - u);
        CCPoint pos = bezier(p.from, p.ctrl, p.to, e);
        float radius = kParticleRadius * (1.f - 0.3f * u);
        m_particles->drawCircle(pos, radius + 1.f, { 0.f, 0.f, 0.f, 1.f }, 0.f, kNoBorder, kDotSegments);
        m_particles->drawCircle(pos, radius, opaque(p.gold ? kGoldColor : kFillColor), 0.f, kNoBorder, kDotSegments);
    }
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
        if (r.t >= r.mergeAt) m_goldTint = lerp(kGoldColor, kFillColor, std::min(1.f, (r.t - r.mergeAt) / kMergeTime));
        this->layoutFill();
        if (r.t >= r.fillAt) {
            // against the cost this death was filling toward, so an earned draft ends on e.g. 30/30
            std::string text = fmt::format("{:.0f}/{:.0f}", std::round(m_goldShown * r.threshold), r.threshold);
            this->showGaugeText(text);
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
    m_goldTint = kGoldColor;
    this->layoutFill();
    this->showGaugeText(m_gaugeText);
}

void RunHud::settleGauge() {
    if (!m_reward.active) return;
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

    // empty box where an icon would go
    auto frame = CCLayerColor::create({ 255, 255, 255, 110 }, kIconBox, kIconBox);
    frame->setPosition({ 0.f, -kIconBox });
    s.root->addChild(frame);
    auto inner = CCLayerColor::create({ 0, 0, 0, 150 }, kIconBox - 2.f, kIconBox - 2.f);
    inner->setPosition({ 1.f, -kIconBox + 1.f });
    s.root->addChild(inner);

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
    // takes the oldest label; whatever is still up moves a line higher
    auto& toast = m_toasts[m_nextToast];
    m_nextToast = (m_nextToast + 1) % m_toasts.size();
    for (auto& other : m_toasts) {
        if (&other != &toast && other.age >= 0.f) other.line += 1.f;
    }
    toast.label->setString(text.c_str());
    toast.age = 0.f;
    toast.line = 0.f;
    toast.lineShown = 0.f;
    this->stepToasts(0.f);
}

// Not cocos actions: CCMoveBy sets an absolute position every frame, so the
// push-up and a line's own rise/sink would fight over the label.
void RunHud::stepToasts(float dt) {
    for (auto& toast : m_toasts) {
        if (toast.age < 0.f) continue;
        toast.age += dt;
        float lift = 0.f;
        if (!envelope(toast.age, kToastIn, kToastHold, kToastOut, lift)) {
            toast.age = -1.f;
            toast.label->setOpacity(0);
            continue;
        }
        toast.lineShown += (toast.line - toast.lineShown) * std::min(1.f, dt * kToastStackRate);
        float const y = kToastY + toast.lineShown * kToastStep - (1.f - lift) * kToastRise;
        toast.label->setPosition({ kToastX, y });
        toast.label->setOpacity(static_cast<GLubyte>(std::lround(255.f * lift)));
    }
}

void RunHud::banner(std::string const& text) {
    m_banner.label->setString(text.c_str());
    m_banner.age = 0.f;
    this->stepBanner(0.f);
}

void RunHud::stepBanner(float dt) {
    if (m_banner.age < 0.f) return;
    m_banner.age += dt;
    float lift = 0.f;
    if (!envelope(m_banner.age, kBannerIn, kBannerHold, kBannerOut, lift)) {
        m_banner.age = -1.f;
        m_banner.label->setOpacity(0);
        return;
    }
    auto const winSize = CCDirector::get()->getWinSize();
    float const gaugeTop = m_gaugeTop > 0.f ? m_gaugeTop : 40.f;
    float const restY = gaugeTop + kBannerGap + m_banner.label->getScaledContentSize().height / 2;
    m_banner.label->setPosition({ winSize.width / 2, restY - (1.f - lift) * kBannerRise });
    m_banner.label->setOpacity(static_cast<GLubyte>(std::lround(255.f * lift)));
}

} // namespace augment

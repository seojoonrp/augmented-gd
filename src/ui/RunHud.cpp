#include "RunHud.hpp"
#include "Fonts.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kMargin = 6.f;

    // Gauge: GD's own progress-bar sprite, right under the real one.
    constexpr char const* kBarSprite = "GJ_progressBar_001.png";
    constexpr float kFallbackInset = 2.5f;   // when GD's fill can't be measured
    constexpr float kFallbackScale = 0.6f;   // no GD bar to copy: the file is 340x20
    constexpr float kGaugeEase = 8.f;        // per second; ~0.4 s to settle

    // Augment column at the far left.
    constexpr float kColumnX = kMargin;
    constexpr float kHeaderScale = 0.36f;
    constexpr float kRowHeight = 15.f;
    constexpr float kRowScale = 0.42f;
    constexpr float kIconBox = 12.f;
    constexpr float kIconGap = 4.f;
    constexpr float kStateGap = 5.f;

    constexpr ccColor3B kFillColor = { 122, 222, 45 };   // the draft cards' GJ_button_01 green
    constexpr ccColor3B kStateColor = { 190, 190, 200 };
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

    // Notices are player-facing (UI font); everything else is a debug readout.
    m_notice = CCLabelBMFont::create("", fonts::Name);
    m_notice->setScale(0.7f);
    m_notice->setPosition({ winSize.width / 2, winSize.height * 0.7f });
    m_notice->setOpacity(0);
    m_notice->setID("notice");
    this->addChild(m_notice, 1);

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

    m_gaugeFill = CCLayerColor::create({ kFillColor.r, kFillColor.g, kFillColor.b, 255 }, 0.f, fillHeight);
    m_gaugeFill->setPosition({ m_trackLeft, fillBottom });
    m_gaugeFill->setID("gauge-fill");
    m_gaugeBar->addChild(m_gaugeFill, -1);

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

void RunHud::setGauge(float value, float threshold) {
    m_gaugeTarget = threshold > 0.f ? std::clamp(value / threshold, 0.f, 1.f) : 0.f;
    std::string text = fmt::format("{:.0f}/{:.0f}", std::min(value, threshold), threshold);
    if (text != m_gaugeLabel->getString()) m_gaugeLabel->setString(text.c_str());
}

void RunHud::update(float dt) {
    if (!m_gaugeFill) return;
    float diff = m_gaugeTarget - m_gaugeShown;
    if (std::fabs(diff) < 0.002f) {
        if (diff == 0.f) return;
        m_gaugeShown = m_gaugeTarget;
    }
    else {
        m_gaugeShown += diff * std::min(1.f, dt * kGaugeEase);
    }
    m_gaugeFill->setContentSize({ std::round(m_trackWidth * m_gaugeShown), m_gaugeFill->getContentSize().height });
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

void RunHud::notice(std::string const& text, ccColor3B color) {
    m_notice->setString(text.c_str());
    m_notice->setColor(color);
    m_notice->stopAllActions();
    m_notice->setOpacity(255);
    m_notice->runAction(CCSequence::create(
        CCDelayTime::create(1.f),
        CCFadeOut::create(0.5f),
        nullptr
    ));
}

} // namespace augment

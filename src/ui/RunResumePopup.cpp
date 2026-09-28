#include "RunResumePopup.hpp"
#include "Fonts.hpp"
#include "CardStyle.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment {

namespace {
    // The run summary's blue card (card::framedPanel) floating over the
    // dimmed level info screen, the two buttons inside it along the bottom
    // (user, 2026-09-28: it was green with the buttons under it).
    constexpr float kCardWidth = 340.f;
    constexpr float kPadTop = 20.f;       // card top -> title top
    constexpr float kPadBottom = 16.f;    // buttons bottom -> card bottom
    constexpr float kTitleGap = 14.f;     // title -> message
    constexpr float kButtonGap = 18.f;    // message -> buttons
    constexpr float kButtonSpacing = 16.f;

    constexpr float kTitleScale = 0.8f;
    constexpr float kBodyScale = 0.75f;
    constexpr float kButtonScale = 0.8f;
}

RunResumePopup* RunResumePopup::create(Callback onRestart, Callback onContinue) {
    auto ret = new RunResumePopup();
    if (ret->init(std::move(onRestart), std::move(onContinue))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool RunResumePopup::init(Callback onRestart, Callback onContinue) {
    m_onRestart = std::move(onRestart);
    m_onContinue = std::move(onContinue);

    // Build the pieces first, then size the popup around them.
    auto title = CCLabelBMFont::create("증강 모드", fonts::Name);
    title->setScale(kTitleScale);

    auto body = CCLabelBMFont::create(
        "이전에 진행한 증강 세션이 있습니다.\n이어서 하시겠습니까?",
        fonts::Text, kCCLabelAutomaticWidth, kCCTextAlignmentCenter
    );
    body->setScale(kBodyScale);

    auto restartSpr = ButtonSprite::create("Restart", "goldFont.fnt", "GJ_button_06.png", kButtonScale);
    auto continueSpr = ButtonSprite::create("Continue", "goldFont.fnt", "GJ_button_01.png", kButtonScale);

    float const titleH = title->getScaledContentSize().height;
    float const bodyH = body->getScaledContentSize().height;
    float const buttonH = std::max(restartSpr->getScaledContentSize().height, continueSpr->getScaledContentSize().height);
    float const height = kPadTop + titleH + kTitleGap + bodyH + kButtonGap + buttonH + kPadBottom;

    if (!Popup::init(kCardWidth, height)) return false;
    this->setID("run-resume-popup"_spr);
    // Floating content like the draft: no brown box, darker overlay. The
    // close button stays on the card's top-left corner.
    m_bgSprite->setVisible(false);
    this->setOpacity(160);

    float const cx = kCardWidth / 2;
    auto panel = card::framedPanel({ kCardWidth, height });
    panel->setPosition({ cx, height / 2 });
    m_mainLayer->addChild(panel, 0);

    float y = height - kPadTop;
    title->setPosition({ cx, y - titleH / 2 });
    y -= titleH + kTitleGap;
    body->setPosition({ cx, y - bodyH / 2 });
    m_mainLayer->addChild(title, 1);
    m_mainLayer->addChild(body, 1);

    auto restartBtn = CCMenuItemExt::createSpriteExtra(restartSpr, [this](auto) { this->choose(true); });
    auto continueBtn = CCMenuItemExt::createSpriteExtra(continueSpr, [this](auto) { this->choose(false); });
    restartBtn->setID("restart");
    continueBtn->setID("continue");
    float const rw = restartSpr->getScaledContentSize().width;
    float const cw = continueSpr->getScaledContentSize().width;
    float const rowLeft = cx - (rw + kButtonSpacing + cw) / 2;
    float const by = kPadBottom + buttonH / 2;
    restartBtn->setPosition({ rowLeft + rw / 2, by });
    continueBtn->setPosition({ rowLeft + rw + kButtonSpacing + cw / 2, by });
    m_buttonMenu->addChild(restartBtn);
    m_buttonMenu->addChild(continueBtn);

    log::info("RunResumePopup: card {}x{}, buttons inside", kCardWidth, height);
    return true;
}

void RunResumePopup::choose(bool restart) {
    // Popup::onClose removes us; keep the callback alive past that.
    auto cb = restart ? m_onRestart : m_onContinue;
    log::info("RunResumePopup: {}", restart ? "restart" : "continue");
    Popup::onClose(nullptr);
    if (cb) cb();
}

} // namespace augment

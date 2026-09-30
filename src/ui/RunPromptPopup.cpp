#include "RunPromptPopup.hpp"
#include "Fonts.hpp"
#include "CardStyle.hpp"
#include "../game/Language.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kCardWidth = 340.f;
    constexpr float kPadTop = 20.f;
    constexpr float kPadSide = 18.f;      // card side -> widest message line
    constexpr float kPadBottom = 16.f;    // buttons bottom -> card bottom
    constexpr float kTitleGap = 14.f;     // title -> message
    constexpr float kButtonGap = 18.f;    // message -> buttons
    constexpr float kButtonSpacing = 16.f;

    constexpr float kTitleScale = 0.8f;
    constexpr float kBodyScale = 0.75f;
    constexpr float kBodyMinScale = 0.45f;
    constexpr float kButtonScale = 0.8f;

    // labels stay English in both languages, goldFont has no Hangul
    ButtonSprite* buttonSprite(RunPromptPopup::Button const& button) {
        return ButtonSprite::create(button.label, "goldFont.fnt", button.texture, kButtonScale);
    }
}

RunPromptPopup* RunPromptPopup::resume(Callback onRestart, Callback onContinue) {
    auto ret = new RunPromptPopup();
    bool const ok = ret->init(
        "resume",
        tr("Augment Mode", "증강 모드"),
        tr("You have a run in progress on this level.\nPick up where you left off?",
           "이전에 진행한 증강 세션이 있습니다.\n이어서 하시겠습니까?"),
        { "Restart", "GJ_button_06.png", std::move(onRestart) },
        { "Continue", "GJ_button_01.png", std::move(onContinue) }
    );
    if (ok) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

RunPromptPopup* RunPromptPopup::replace(std::string const& otherLevel, Callback onStart) {
    auto ret = new RunPromptPopup();
    bool const ok = ret->init(
        "replace",
        tr("Run in Progress", "진행 중인 증강 세션"),
        fmt::format(
            fmt::runtime(tr("You have a run in progress on\n'{}'.\nStarting a new run here will end it.",
                            "'{}' 레벨에서 진행 중인\n증강 세션이 있습니다.\n여기서 새로 시작하면 그 세션은 사라집니다.")),
            otherLevel
        ),
        { "Cancel", "GJ_button_06.png", nullptr },
        { "Start", "GJ_button_01.png", std::move(onStart) }
    );
    if (ok) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool RunPromptPopup::init(char const* name, std::string const& titleText, std::string const& message, Button left, Button right) {
    m_name = name;
    m_left = std::move(left);
    m_right = std::move(right);

    // build the pieces first, then size the popup around them
    auto title = CCLabelBMFont::create(titleText.c_str(), fonts::Name);
    title->setScale(kTitleScale);

    auto body = CCLabelBMFont::create(message.c_str(), fonts::Text, kCCLabelAutomaticWidth, kCCTextAlignmentCenter);
    // long level names
    body->limitLabelWidth(kCardWidth - 2 * kPadSide, kBodyScale, kBodyMinScale);

    auto leftSpr = buttonSprite(m_left);
    auto rightSpr = buttonSprite(m_right);

    float const titleH = title->getScaledContentSize().height;
    float const bodyH = body->getScaledContentSize().height;
    float const buttonH = std::max(leftSpr->getScaledContentSize().height, rightSpr->getScaledContentSize().height);
    float const height = kPadTop + titleH + kTitleGap + bodyH + kButtonGap + buttonH + kPadBottom;

    if (!Popup::init(kCardWidth, height)) return false;
    this->setID(fmt::format("run-prompt-{}", m_name));
    // no brown box, darker overlay; the close button on the card's corner cancels
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

    auto leftBtn = CCMenuItemExt::createSpriteExtra(leftSpr, [this](auto) { this->choose(m_left); });
    auto rightBtn = CCMenuItemExt::createSpriteExtra(rightSpr, [this](auto) { this->choose(m_right); });
    leftBtn->setID("left-button");
    rightBtn->setID("right-button");
    float const lw = leftSpr->getScaledContentSize().width;
    float const rw = rightSpr->getScaledContentSize().width;
    float const rowLeft = cx - (lw + kButtonSpacing + rw) / 2;
    float const by = kPadBottom + buttonH / 2;
    leftBtn->setPosition({ rowLeft + lw / 2, by });
    rightBtn->setPosition({ rowLeft + lw + kButtonSpacing + rw / 2, by });
    m_buttonMenu->addChild(leftBtn);
    m_buttonMenu->addChild(rightBtn);
    return true;
}

void RunPromptPopup::choose(Button const& button) {
    // onClose releases us (and the button), so copy the callback first
    auto cb = button.onClick;
    Popup::onClose(nullptr);
    if (cb) cb();
}

} // namespace augment

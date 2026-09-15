#include "AugmentDraftPopup.hpp"
#include "../core/AugmentManager.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kCardWidth = 110.f;
    constexpr float kCardHeight = 150.f;
    constexpr float kCardGap = 12.f;
    constexpr float kPopupPadding = 24.f;
    constexpr float kTitleSpace = 30.f;
}

AugmentDraftPopup* AugmentDraftPopup::create(std::vector<AugmentDef const*> choices, PickCallback onPick) {
    auto ret = new AugmentDraftPopup();
    if (ret->init(std::move(choices), std::move(onPick))) {
        ret->autorelease();
        return ret;
    }
    log::error("AugmentDraftPopup::init failed");
    delete ret;
    return nullptr;
}

bool AugmentDraftPopup::init(std::vector<AugmentDef const*> choices, PickCallback onPick) {
    m_choices = std::move(choices);
    m_onPick = std::move(onPick);

    auto const n = static_cast<float>(m_choices.size());
    float width = n * kCardWidth + (n - 1) * kCardGap + kPopupPadding * 2;
    float height = kCardHeight + kTitleSpace + kPopupPadding;
    if (!Popup::init(width, height)) {
        log::error("Popup::init failed");
        return false;
    }

    this->setTitle("Choose an Augment");

    // No way out but picking a card: drop the close button (Popup adds it to
    // m_buttonMenu, and we're about to lay that menu out as a card row).
    m_closeBtn->removeFromParentAndCleanup(true);
    m_closeBtn = nullptr;

    auto& mgr = AugmentManager::get();
    for (size_t i = 0; i < m_choices.size(); i++) {
        auto const& def = *m_choices[i];
        auto card = this->createCard(def, mgr.levelOf(def.id));
        auto item = CCMenuItemSpriteExtra::create(card, this, menu_selector(AugmentDraftPopup::onCard));
        item->setTag(static_cast<int>(i));
        item->setID(fmt::format("card-{}", i));
        m_buttonMenu->addChild(item);
    }

    m_buttonMenu->setLayout(
        RowLayout::create()
            ->setGap(kCardGap)
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::Center)
            ->setCrossAxisAlignment(AxisAlignment::Center)
    );
    // Push the row down a bit so it sits under the title.
    m_buttonMenu->setContentHeight(m_size.height - kTitleSpace);
    m_buttonMenu->updateLayout();

    return true;
}

CCNode* AugmentDraftPopup::createCard(AugmentDef const& def, int currentLevel) {
    auto bg = CCScale9Sprite::create("GJ_square02.png");
    bg->setContentSize({ kCardWidth, kCardHeight });

    auto name = CCLabelBMFont::create(def.name.c_str(), "bigFont.fnt");
    name->limitLabelWidth(kCardWidth - 16.f, 0.5f, 0.2f);
    bg->addChildAtPosition(name, Anchor::Top, { 0.f, -18.f });

    int const nextLevel = std::min(currentLevel + 1, def.maxLevel());
    std::string levelText = currentLevel == 0
        ? fmt::format("NEW  Lv {}", nextLevel)
        : fmt::format("Lv {} -> {}", currentLevel, nextLevel);
    auto level = CCLabelBMFont::create(levelText.c_str(), "goldFont.fnt");
    level->setScale(0.45f);
    bg->addChildAtPosition(level, Anchor::Top, { 0.f, -38.f });

    auto desc = CCLabelBMFont::create(
        def.describe(nextLevel).c_str(), "chatFont.fnt",
        (kCardWidth - 16.f) / 0.6f, kCCTextAlignmentCenter
    );
    desc->setScale(0.6f);
    desc->setAnchorPoint({ 0.5f, 1.f });
    bg->addChildAtPosition(desc, Anchor::Top, { 0.f, -54.f });

    return bg;
}

void AugmentDraftPopup::onCard(CCObject* sender) {
    auto idx = static_cast<size_t>(sender->getTag());
    if (idx >= m_choices.size()) return;

    // Copy what we need, tear the popup down, *then* notify. Popup::onClose
    // removes us from the parent, which may free `this`.
    auto id = m_choices[idx]->id;
    auto cb = std::move(m_onPick);

    log::info("Draft pick: {}", id);
    Popup::onClose(nullptr);
    if (cb) cb(id);
}

} // namespace augment

#include "AugmentDraftPopup.hpp"
#include "Fonts.hpp"
#include "../core/AugmentManager.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment {

namespace {
    // Card geometry for a three-card draft, sized for the Korean descriptions:
    // the longest wraps to ~6 lines at kDescScale. Three cards + padding =
    // 492 pt, inside GD's 569 pt width.
    constexpr float kCardWidth = 140.f;
    constexpr float kCardHeight = 180.f;
    constexpr float kCardGap = 12.f;
    constexpr float kPopupPadding = 24.f;
    constexpr float kTitleSpace = 30.f;
    constexpr float kCardInset = 8.f;
    constexpr float kNameScale = 0.55f;
    constexpr float kDescScale = 0.55f;
    // Four cards (draft-count) at full width would need 644 pt, so they are
    // narrowed to fit. Leaves a margin inside the 569 pt screen.
    constexpr float kMaxPopupWidth = 540.f;

    // Everything drawn inside a card, so a narrower card can shrink its
    // contents by the same ratio instead of overflowing.
    struct CardLayout {
        float width = kCardWidth;
        float scale = 1.f;

        float nameScale() const { return kNameScale * scale; }
        float descScale() const { return kDescScale * scale; }
        // Wrap width is in font units, i.e. pre-scale.
        float textWidth() const { return (width - 2 * kCardInset) / this->descScale(); }
    };

    CardLayout layoutFor(size_t count) {
        CardLayout layout;
        float n = static_cast<float>(count);
        float avail = kMaxPopupWidth - 2 * kPopupPadding - (n - 1) * kCardGap;
        if (n * layout.width <= avail) return layout;

        layout.width = avail / n;
        // Shrinking the type by the same ratio keeps the wrap width (in font
        // units) about the same, so the line count barely moves while every
        // line gets shorter — the text block ends up smaller, not taller, and
        // the card keeps its full height.
        layout.scale = layout.width / kCardWidth;
        return layout;
    }
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

    auto const layout = layoutFor(m_choices.size());
    auto const n = static_cast<float>(m_choices.size());
    float width = n * layout.width + (n - 1) * kCardGap + kPopupPadding * 2;
    float height = kCardHeight + kTitleSpace + kPopupPadding;
    if (!Popup::init(width, height)) {
        log::error("Popup::init failed");
        return false;
    }

    log::info(
        "Draft popup: {} cards, {:.0f}x{:.0f} each, popup {:.0f}x{:.0f}",
        m_choices.size(), layout.width, kCardHeight, width, height
    );

    this->setTitle("증강 선택", fonts::Name, 0.7f);

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
    // Stateless: every card in one popup sees the same choice count.
    auto const l = layoutFor(m_choices.size());

    auto bg = CCScale9Sprite::create("GJ_square02.png");
    bg->setContentSize({ l.width, kCardHeight });

    auto name = CCLabelBMFont::create(def.name.c_str(), fonts::Name);
    name->setExtraKerning(fonts::NameKerning);
    name->limitLabelWidth(l.width - 2 * kCardInset, l.nameScale(), 0.2f);
    bg->addChildAtPosition(name, Anchor::Top, { 0.f, -18.f * l.scale });

    // Level line stays GD-style (digits only, so goldFont is fine).
    int const nextLevel = std::min(currentLevel + 1, def.maxLevel);
    std::string levelText = currentLevel == 0
        ? fmt::format("NEW  Lv {}", nextLevel)
        : fmt::format("Lv {} -> {}", currentLevel, nextLevel);
    auto level = CCLabelBMFont::create(levelText.c_str(), "goldFont.fnt");
    level->setScale(0.45f * l.scale);
    bg->addChildAtPosition(level, Anchor::Top, { 0.f, -38.f * l.scale });

    // Width is in font units (pre-scale). CCLabelBMFont wraps at spaces, which
    // Korean has between words; the explicit newlines in the text also break.
    auto desc = CCLabelBMFont::create(
        def.describe(nextLevel).c_str(), fonts::Text,
        l.textWidth(), kCCTextAlignmentCenter
    );
    desc->setScale(l.descScale());
    desc->setAnchorPoint({ 0.5f, 1.f });
    bg->addChildAtPosition(desc, Anchor::Top, { 0.f, -54.f * l.scale });

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

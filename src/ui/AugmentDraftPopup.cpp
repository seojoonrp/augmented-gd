#include "AugmentDraftPopup.hpp"
#include "AugmentCard.hpp"
#include "Fonts.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/Language.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {
    // Card geometry (sd points) for a three-card draft. The card is drawn at
    // full size (AugmentCard.hpp) and scaled down as a whole when four cards
    // have to fit.
    constexpr float kCardWidth = card::CardWidth;
    constexpr float kCardHeight = card::CardHeight;
    constexpr float kCardGap = 12.f;
    constexpr float kPopupPadding = 16.f;
    constexpr float kTitleSpace = 36.f;
    // Four cards (draft-count) at full width would need 644 pt, so they are
    // narrowed to fit. Leaves a margin inside the 569 pt screen.
    constexpr float kMaxPopupWidth = 540.f;
    constexpr float kTitleScale = 1.f;

    // Reveal: cards start stacked at the row centre, small and slightly
    // fanned, and ease out to their slots one after another.
    constexpr float kRevealDuration = 0.45f;
    constexpr float kRevealStagger = 0.08f;
    constexpr float kRevealFromScale = 0.25f;
    constexpr float kRevealFanDegrees = 10.f;

    float cardScaleFor(size_t count) {
        float n = static_cast<float>(count);
        float avail = kMaxPopupWidth - 2 * kPopupPadding - (n - 1) * kCardGap;
        return std::min(1.f, avail / (n * kCardWidth));
    }

    float easeBackOut(float t) {
        constexpr float s = 1.3f;  // overshoot; cocos' default 1.70158 is too bouncy for cards
        t -= 1.f;
        return t * t * ((s + 1.f) * t + s) + 1.f;
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
    m_cardScale = cardScaleFor(m_choices.size());

    auto const n = static_cast<float>(m_choices.size());
    float const cardW = kCardWidth * m_cardScale;
    float const cardH = kCardHeight * m_cardScale;
    float const width = n * cardW + (n - 1) * kCardGap + kPopupPadding * 2;
    float const height = cardH + kTitleSpace + kPopupPadding;
    if (!Popup::init(width, height)) {
        log::error("Popup::init failed");
        return false;
    }

    // The cards float over the dimmed level instead of sitting in a brown
    // box, so hide the popup's own background and darken the overlay.
    m_bgSprite->setVisible(false);
    this->setOpacity(160);
    this->setTitle(tr("Choose an Augment", "증강 선택"), fonts::Name, kTitleScale);

    // No way out but picking a card: drop the close button.
    m_closeBtn->removeFromParentAndCleanup(true);
    m_closeBtn = nullptr;

    // Row of cards under the title, positioned by hand (the reveal needs
    // fixed slots, and a layout would re-place them).
    float const rowY = (m_size.height - kTitleSpace) / 2;
    float const rowLeft = (m_size.width - (n * cardW + (n - 1) * kCardGap)) / 2;
    auto& mgr = AugmentManager::get();
    for (size_t i = 0; i < m_choices.size(); i++) {
        auto const& def = *m_choices[i];

        // holder: the item's fixed footprint. visual: the full-size card,
        // scaled to fit, which the reveal animates inside the holder.
        auto holder = CCNode::create();
        holder->setContentSize({ cardW, cardH });
        auto visual = this->createCard(def, mgr.levelOf(def.id));
        visual->setScale(m_cardScale);
        visual->setPosition({ cardW / 2, cardH / 2 });
        holder->addChild(visual);

        auto item = CCMenuItemSpriteExtra::create(holder, this, menu_selector(AugmentDraftPopup::onCard));
        item->m_scaleMultiplier = 1.05f;
        item->setTag(static_cast<int>(i));
        item->setID(fmt::format("card-{}", i));
        float const itemX = rowLeft + cardW / 2 + static_cast<float>(i) * (cardW + kCardGap);
        item->setPosition({ itemX, rowY });
        m_buttonMenu->addChild(item);

        RevealCard rc;
        rc.visual = visual;
        rc.to = ccp(cardW / 2, cardH / 2);
        rc.from = ccp(m_size.width / 2 - itemX + cardW / 2, cardH / 2);
        rc.fromRotation = (static_cast<float>(i) - (n - 1) / 2) * -kRevealFanDegrees;
        m_reveal.push_back(rc);
    }

    // Cards are not pickable until they have landed.
    m_buttonMenu->setEnabled(false);
    m_revealing = true;
    m_revealStart = std::chrono::steady_clock::now();
    this->stepReveal();
    return true;
}

// What a pick would give: the next level's draft text, "NEW" or the level
// change in the footer, and the stars of the level held *now* (all empty on
// a new augment).
CCNode* AugmentDraftPopup::createCard(AugmentDef const& def, int currentLevel) {
    int const nextLevel = std::min(currentLevel + 1, def.maxLevel);
    Lang const lang = language();
    card::CardFace face;
    face.name = def.name.in(lang);
    face.description = def.describe(nextLevel, lang);
    face.footer = currentLevel == 0 ? "NEW" : fmt::format("Lv {} → {}", currentLevel, nextLevel);
    face.pips = currentLevel;
    return card::augmentCard(def, face);
}

void AugmentDraftPopup::visit() {
    if (m_revealing) this->stepReveal();
    Popup::visit();
}

void AugmentDraftPopup::stepReveal() {
    using namespace std::chrono;
    float const elapsed = duration<float>(steady_clock::now() - m_revealStart).count();
    bool done = true;
    for (size_t i = 0; i < m_reveal.size(); i++) {
        auto& rc = m_reveal[i];
        float t = (elapsed - static_cast<float>(i) * kRevealStagger) / kRevealDuration;
        t = std::clamp(t, 0.f, 1.f);
        if (t < 1.f) done = false;
        float const e = easeBackOut(t);
        rc.visual->setPosition(rc.from + (rc.to - rc.from) * e);
        rc.visual->setScale(m_cardScale * (kRevealFromScale + (1.f - kRevealFromScale) * e));
        rc.visual->setRotation(rc.fromRotation * (1.f - e));
    }
    if (done) {
        m_revealing = false;
        m_buttonMenu->setEnabled(true);
    }
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

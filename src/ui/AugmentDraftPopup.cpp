#include "AugmentDraftPopup.hpp"
#include "Fonts.hpp"
#include "../core/AugmentManager.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/NineSlice.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {
    // Card geometry (sd points) for a three-card draft. The card is drawn at
    // full size and scaled down as a whole when four cards have to fit.
    constexpr float kCardWidth = 140.f;
    constexpr float kCardHeight = 210.f;
    constexpr float kCardGap = 12.f;
    constexpr float kPopupPadding = 16.f;
    constexpr float kTitleSpace = 36.f;
    // Four cards (draft-count) at full width would need 644 pt, so they are
    // narrowed to fit. Leaves a margin inside the 569 pt screen.
    constexpr float kMaxPopupWidth = 540.f;

    // GD button look: white rim, then GJ_button_01 (black ring + flat green).
    constexpr float kRim = 2.f;
    constexpr float kRimRadius = 8.f;                   // just outside GJ_button_01's ~6 pt corners
    constexpr float kRing = 2.5f;                       // GJ_button_01's black ring at sd
    constexpr ccColor3B kBodyGreen = { 122, 222, 45 };  // GJ_button_01's fill
    constexpr float kInset = 10.f;

    // Vertical slots, measured down from the card's top edge.
    constexpr float kNameY = 18.f;
    constexpr float kNameScale = 0.6f;
    constexpr float kImageTop = 34.f;
    constexpr float kImageWidth = 120.f;
    constexpr float kImageHeight = 70.f;
    constexpr float kImageBorder = 2.f;
    constexpr float kImageRadius = 4.5f;
    // Description block is centred between the image box and the footer band,
    // with this much breathing room above and below.
    constexpr float kDescMargin = 5.f;
    constexpr float kDescScale = 0.5f;
    constexpr float kDescMinScale = 0.35f;
    constexpr float kFooterHeight = 20.f;
    constexpr float kFooterRadius = 4.f;
    constexpr float kFooterScale = 0.5f;
    constexpr float kTitleScale = 1.f;

    // square02b_001's corner radius at scale 1 (measured: ~36 of 320 uhd px).
    constexpr float kSquareRadius = 9.f;

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

    // square02b_001 is a plain white rounded square; the slices are scaled so
    // the corner radius comes out as asked, whatever the box size.
    NineSlice* roundedBox(CCSize size, ccColor3B color, float radius, GLubyte opacity = 255) {
        float const scale = radius / kSquareRadius;
        auto box = NineSlice::create("square02b_001.png");
        box->setScale(scale);
        box->setContentSize(size / scale);
        box->setColor(color);
        box->setOpacity(opacity);
        return box;
    }

    // Word-wrap by measuring words with throwaway labels. CCLabelBMFont's
    // width argument never wrapped our fonts in game (Pretendard or the baked
    // ImcreSoojin, 2026-09-17), so the label gets explicit newlines instead.
    // `maxWidth` is in label units (pre-scale). Existing newlines are kept.
    std::string wrapText(std::string const& text, char const* font, float maxWidth) {
        auto measure = [&](std::string const& s) {
            auto probe = CCLabelBMFont::create(s.c_str(), font);
            return probe ? probe->getContentSize().width : 0.f;
        };
        std::string out;
        size_t paraStart = 0;
        while (paraStart <= text.size()) {
            size_t paraEnd = text.find('\n', paraStart);
            if (paraEnd == std::string::npos) paraEnd = text.size();
            std::string line;
            size_t wordStart = paraStart;
            while (wordStart <= paraEnd) {
                size_t wordEnd = text.find(' ', wordStart);
                if (wordEnd == std::string::npos || wordEnd > paraEnd) wordEnd = paraEnd;
                auto word = text.substr(wordStart, wordEnd - wordStart);
                if (!word.empty()) {
                    auto candidate = line.empty() ? word : line + " " + word;
                    if (!line.empty() && measure(candidate) > maxWidth) {
                        out += line + '\n';
                        line = word;
                    }
                    else {
                        line = candidate;
                    }
                }
                wordStart = wordEnd + 1;
            }
            out += line;
            if (paraEnd < text.size()) out += '\n';
            paraStart = paraEnd + 1;
        }
        return out;
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
    this->setTitle("증강 선택", fonts::Name, kTitleScale);

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

CCNode* AugmentDraftPopup::createCard(AugmentDef const& def, int currentLevel) {
    auto card = CCNode::create();
    card->setContentSize({ kCardWidth, kCardHeight });
    card->setAnchorPoint({ 0.5f, 0.5f });
    auto const centre = CCPoint{ kCardWidth / 2, kCardHeight / 2 };
    auto const fromTop = [](float dy) { return CCPoint{ kCardWidth / 2, kCardHeight - dy }; };
    float const inner = kCardWidth - 2 * kInset;

    // White rim (full-size slices: its corner radius must cover the body's).
    auto rim = roundedBox({ kCardWidth + 2 * kRim, kCardHeight + 2 * kRim }, ccWHITE, kRimRadius);
    rim->setPosition(centre);
    card->addChild(rim, 0);

    // Body: GD's green button, black ring and highlight included.
    auto body = NineSlice::create("GJ_button_01.png");
    body->setContentSize({ kCardWidth, kCardHeight });
    body->setPosition(centre);
    card->addChild(body, 1);

    // Footer band: the darker strip along the bottom of GD's big buttons.
    auto band = roundedBox({ kCardWidth - 2 * kRing, kFooterHeight }, ccBLACK, kFooterRadius, 70);
    band->setPosition({ kCardWidth / 2, kRing + kFooterHeight / 2 });
    card->addChild(band, 2);

    auto name = CCLabelBMFont::create(def.name.c_str(), fonts::Name);
    name->limitLabelWidth(inner, kNameScale, 0.3f);
    name->setPosition(fromTop(kNameY));
    card->addChild(name, 3);

    // Image slot: black border around a white panel, empty until the art
    // exists. The inner radius is the outer one minus the border so the
    // border looks the same thickness around the corners.
    auto const imageCentre = fromTop(kImageTop + kImageHeight / 2);
    auto imageFrame = roundedBox({ kImageWidth, kImageHeight }, ccBLACK, kImageRadius);
    imageFrame->setPosition(imageCentre);
    card->addChild(imageFrame, 2);
    auto imageFill = roundedBox(
        { kImageWidth - 2 * kImageBorder, kImageHeight - 2 * kImageBorder }, ccWHITE,
        kImageRadius - kImageBorder
    );
    imageFill->setPosition(imageCentre);
    card->addChild(imageFill, 3);

    // Description: wrapped here (see wrapText) at the card's inner width and
    // shrunk in steps until it fits between the image box and the footer,
    // then centred in that gap.
    int const nextLevel = std::min(currentLevel + 1, def.maxLevel);
    float const slotTop = kImageTop + kImageHeight + kDescMargin;
    float const slotBottom = kCardHeight - kRing - kFooterHeight - kDescMargin;
    float const slot = slotBottom - slotTop;
    float descScale = kDescScale;
    CCLabelBMFont* desc = nullptr;
    for (;;) {
        auto wrapped = wrapText(def.describe(nextLevel), fonts::Text, inner / descScale);
        desc = CCLabelBMFont::create(
            wrapped.c_str(), fonts::Text, kCCLabelAutomaticWidth, kCCTextAlignmentCenter
        );
        if (desc->getContentSize().height * descScale <= slot || descScale <= kDescMinScale + 1e-3f) break;
        descScale -= 0.05f;
    }
    if (desc->getContentSize().height * descScale > slot) {
        log::warn(
            "Draft card '{}' description still overflows at scale {:.2f} ({:.0f} > {:.0f} pt)",
            def.id, descScale, desc->getContentSize().height * descScale, slot
        );
    }
    desc->setScale(descScale);
    desc->setAnchorPoint({ 0.5f, 0.5f });
    desc->setPosition(fromTop((slotTop + slotBottom) / 2));
    card->addChild(desc, 3);

    // Footer: level change on the left, level pips on the right.
    std::string levelText = currentLevel == 0
        ? "NEW"
        : fmt::format("Lv {} → {}", currentLevel, nextLevel);
    auto level = CCLabelBMFont::create(levelText.c_str(), fonts::Text);
    level->setScale(kFooterScale);
    level->setAnchorPoint({ 0.f, 0.5f });
    level->setPosition({ kInset, band->getPositionY() });
    card->addChild(level, 3);

    // Level pips show the level held *now* (all empty on a new augment). Two
    // labels because a label has one colour: gold held, grey remaining. Both
    // hug the right edge.
    std::string filled, empty;
    for (int i = 0; i < currentLevel; i++) filled += "★";
    for (int i = currentLevel; i < def.maxLevel; i++) empty += "☆";
    auto emptyPips = CCLabelBMFont::create(empty.c_str(), fonts::Text);
    emptyPips->setScale(kFooterScale);
    emptyPips->setColor({ 150, 150, 150 });
    emptyPips->setAnchorPoint({ 1.f, 0.5f });
    emptyPips->setPosition({ kCardWidth - kInset, band->getPositionY() });
    card->addChild(emptyPips, 3);
    auto filledPips = CCLabelBMFont::create(filled.c_str(), fonts::Text);
    filledPips->setScale(kFooterScale);
    filledPips->setColor({ 255, 215, 60 });
    filledPips->setAnchorPoint({ 1.f, 0.5f });
    filledPips->setPosition({
        kCardWidth - kInset - emptyPips->getContentSize().width * kFooterScale,
        band->getPositionY()
    });
    card->addChild(filledPips, 3);
    return card;
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

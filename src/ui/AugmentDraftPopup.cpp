#include "AugmentDraftPopup.hpp"
#include "AugmentCard.hpp"
#include "CardStyle.hpp"
#include "Fonts.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/Language.hpp"
#include "../game/Sfx.hpp"

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

    // Hover: the card rises this far (pt) and eases at this rate (per
    // second), the run summary's tile hover rate.
    constexpr float kHoverLift = 7.f;
    constexpr float kHoverRate = 18.f;

    // Pick (user, 2026-09-29: a pick used to resume the level at once, and
    // its sound ran into the level music starting): the chosen card glides
    // to the middle of the row and swells while the others shrink and drop
    // away and a ring flashes out from it; at the end it shrinks away as the
    // overlay lifts, and the level resumes kPickDuration after the click.
    constexpr float kPickDuration = 1.f;
    constexpr float kPickMove = 0.35f;     // to the middle, swelling
    constexpr float kPickSwell = 1.12f;
    constexpr float kPickRise = 12.f;      // pt above the row
    constexpr float kPickDismiss = 0.3f;   // the others shrinking away
    constexpr float kPickDrop = 24.f;      // ... and falling this far
    constexpr float kPickRingTime = 0.45f;
    constexpr float kPickRingGrow = 0.18f; // of the card's size
    constexpr float kPickOut = 0.28f;      // the chosen card leaving, the overlay lifting
    constexpr ccColor3B kPickRingColor = { 255, 255, 255 };   // white (gold until the user's word, 2026-09-29)

    constexpr GLubyte kOverlayOpacity = 160;

    float easeOutCubic(float t) {
        float const u = 1.f - t;
        return 1.f - u * u * u;
    }
    float easeInCubic(float t) { return t * t * t; }

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
    this->setOpacity(kOverlayOpacity);
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
        rc.item = item;
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
    if (m_picking) {
        // Closed: out of the scene already, nothing left to draw.
        if (this->stepPick()) return;
    }
    else if (m_revealing) this->stepReveal();
    else this->stepHover();
    Popup::visit();
}

void AugmentDraftPopup::stepHover() {
    using namespace std::chrono;
    auto const now = steady_clock::now();
    float const dt = std::min(0.1f, duration<float>(now - m_lastHover).count());
    m_lastHover = now;

    int hovered = -1;
    auto const mouse = getMousePos();
    for (size_t i = 0; i < m_reveal.size(); i++) {
        auto const p = m_reveal[i].item->convertToNodeSpace(mouse);
        auto const size = m_reveal[i].item->getContentSize();
        if (p.x >= 0.f && p.y >= 0.f && p.x <= size.width && p.y <= size.height) {
            hovered = static_cast<int>(i);
            break;
        }
    }
    if (hovered != m_hovered) {
        if (hovered >= 0) {
            log::info("Draft hover: card {} ('{}')", hovered, m_choices[hovered]->id);
            sfx::play(sfx::Cue::CardHover);
        }
        m_hovered = hovered;
    }

    // A frame-rate-free exponential approach, like the run summary's tiles.
    float const k = 1.f - std::exp(-kHoverRate * dt);
    for (size_t i = 0; i < m_reveal.size(); i++) {
        auto& rc = m_reveal[i];
        float const target = static_cast<int>(i) == m_hovered ? kHoverLift : 0.f;
        if (rc.lift == target) continue;
        rc.lift += (target - rc.lift) * k;
        if (std::abs(rc.lift - target) < 0.01f) rc.lift = target;
        rc.visual->setPosition({ rc.to.x, rc.to.y + rc.lift });
    }
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
        m_lastHover = steady_clock::now();
        m_buttonMenu->setEnabled(true);
    }
}

void AugmentDraftPopup::onCard(CCObject* sender) {
    auto idx = static_cast<size_t>(sender->getTag());
    if (m_picking || idx >= m_choices.size()) return;

    // The pick goes through when the send-off ends (finishPick); until then
    // nothing else can be clicked.
    m_picking = true;
    m_picked = static_cast<int>(idx);
    m_pickStart = std::chrono::steady_clock::now();
    m_buttonMenu->setEnabled(false);
    for (auto& rc : m_reveal) {
        rc.pickFrom = CCPoint(rc.visual->getPositionX(), rc.visual->getPositionY());
        rc.pickFromScale = rc.visual->getScale();
    }
    auto chosen = m_reveal[idx].visual;
    if (auto holder = chosen->getParent()) {
        m_pickRing = CCDrawNode::create();
        holder->addChild(m_pickRing, 1);
    }

    log::info("Draft pick: {} (the level resumes in {:.1f} s)", m_choices[idx]->id, kPickDuration);
    sfx::play(sfx::Cue::CardPick);
    this->stepPick();
}

bool AugmentDraftPopup::stepPick() {
    using namespace std::chrono;
    float const t = duration<float>(steady_clock::now() - m_pickStart).count();
    if (t >= kPickDuration) {
        this->finishPick();
        return true;
    }

    float const outStart = kPickDuration - kPickOut;
    float const out = t > outStart ? (t - outStart) / kPickOut : 0.f;   // 0 -> 1 over the last stretch
    for (size_t i = 0; i < m_reveal.size(); i++) {
        auto& rc = m_reveal[i];
        if (static_cast<int>(i) == m_picked) {
            float const move = std::min(1.f, t / kPickMove);
            CCPoint const target{ rc.from.x, rc.to.y + kPickRise };
            rc.visual->setPosition(rc.pickFrom + (target - rc.pickFrom) * easeOutCubic(move));
            float const swell = 1.f + (kPickSwell - 1.f) * easeBackOut(move);
            rc.visual->setScale(rc.pickFromScale * swell * (1.f - easeInCubic(out)));
            continue;
        }
        float const u = std::min(1.f, t / kPickDismiss);
        rc.visual->setVisible(u < 1.f);
        rc.visual->setScale(rc.pickFromScale * (1.f - easeInCubic(u)));
        rc.visual->setPositionY(rc.pickFrom.y - kPickDrop * easeInCubic(u));
    }

    // The ring: the chosen card's outline, growing out and fading.
    if (m_pickRing) {
        m_pickRing->clear();
        float const r = t / kPickRingTime;
        if (r < 1.f) {
            auto chosen = m_reveal[m_picked].visual;
            float const grow = 1.f + kPickRingGrow * easeOutCubic(r);
            float const w = card::CardWidth * chosen->getScale() * grow;
            float const h = card::CardHeight * chosen->getScale() * grow;
            float const cx = chosen->getPositionX(), cy = chosen->getPositionY();
            auto points = card::roundedRectPoints({ cx - w / 2, cy - h / 2, w, h }, card::RimRadius * chosen->getScale() * grow);
            float const alpha = 1.f - r;
            m_pickRing->drawPolygon(
                points.data(), static_cast<unsigned int>(points.size()),
                card::premul({ 0, 0, 0 }, 0.f), 3.f - 2.f * r, card::premul(kPickRingColor, alpha)
            );
        }
    }

    // The dark overlay and the title lift as the chosen card leaves.
    this->setOpacity(static_cast<GLubyte>(kOverlayOpacity * (1.f - out)));
    if (m_title) m_title->setOpacity(static_cast<GLubyte>(255.f * (1.f - out)));
    return false;
}

void AugmentDraftPopup::finishPick() {
    // This runs from visit(), in the middle of the scene walking its
    // children: Popup::onClose takes us out of them, so keep `this` alive
    // until the frame is over (the autorelease pool drains after the scene
    // is drawn, paused director or not). Copy what we need, close, *then*
    // notify: the callback may open the next draft or resume the level.
    this->retain();
    this->autorelease();
    auto id = m_choices[m_picked]->id;
    auto cb = std::move(m_onPick);
    log::info("Draft pick: '{}' goes through", id);
    Popup::onClose(nullptr);
    if (cb) cb(id);
}

} // namespace augment

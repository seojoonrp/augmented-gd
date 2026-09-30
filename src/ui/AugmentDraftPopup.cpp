#include "AugmentDraftPopup.hpp"
#include "AugButton.hpp"
#include "AugmentCard.hpp"
#include "CardStyle.hpp"
#include "Fonts.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/Language.hpp"
#include "../game/RunSummary.hpp"
#include "../game/Sfx.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kCardWidth = card::CardWidth;
    constexpr float kCardHeight = card::CardHeight;
    constexpr float kCardGap = 12.f;
    constexpr float kPopupPadding = 16.f;
    constexpr float kTitleSpace = 36.f;
    // four full-size cards would need 644, the screen is 569
    constexpr float kMaxPopupWidth = 540.f;
    constexpr float kTitleScale = 1.f;

    // reveal: stacked in the middle, small and fanned, then out to the slots
    constexpr float kRevealDuration = 0.45f;
    constexpr float kRevealStagger = 0.08f;
    constexpr float kRevealFromScale = 0.25f;
    constexpr float kRevealFanDegrees = 10.f;

    constexpr float kHoverLift = 7.f;
    constexpr float kHoverRate = 18.f;    // per second, same as the run summary

    // Pick send-off. The level resumes kPickDuration after the click, which
    // also keeps the pick sound clear of the level music coming back.
    constexpr float kPickDuration = 1.f;
    constexpr float kPickMove = 0.35f;     // to the middle, swelling
    constexpr float kPickSwell = 1.12f;
    constexpr float kPickRise = 12.f;
    constexpr float kPickDismiss = 0.3f;   // the other cards
    constexpr float kPickDrop = 24.f;
    constexpr float kPickRingTime = 0.45f;
    constexpr float kPickRingGrow = 0.18f; // of the card's size
    constexpr float kPickOut = 0.28f;      // card leaves, overlay lifts
    constexpr ccColor3B kPickRingColor = { 255, 255, 255 };

    constexpr GLubyte kOverlayOpacity = 160;

    // run summary button in the top-right corner, a bit smaller than the pause
    // menu's (64 pt there)
    constexpr float kRunButtonSize = 50.f;
    constexpr float kRunButtonMargin = 8.f;

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
        constexpr float s = 1.3f;  // cocos' 1.70158 is too bouncy for cards
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
    if (!Popup::init(width, height)) return false;

    // cards float over the dimmed level, no brown box
    m_bgSprite->setVisible(false);
    this->setOpacity(kOverlayOpacity);
    this->setTitle(tr("Choose an Augment", "증강 선택"), fonts::Name, kTitleScale);

    // no close button, picking is the only way out
    m_closeBtn->removeFromParentAndCleanup(true);
    m_closeBtn = nullptr;

    // placed by hand, a layout would fight the reveal
    float const rowY = (m_size.height - kTitleSpace) / 2;
    float const rowLeft = (m_size.width - (n * cardW + (n - 1) * kCardGap)) / 2;
    auto& mgr = AugmentManager::get();
    for (size_t i = 0; i < m_choices.size(); i++) {
        auto const& def = *m_choices[i];

        // holder = fixed footprint, visual = the scaled card the reveal moves
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

    this->addRunInfoButton();

    // not pickable until they land
    m_buttonMenu->setEnabled(false);
    m_revealing = true;
    m_revealStart = std::chrono::steady_clock::now();
    this->stepReveal();
    return true;
}

// next level's text, but stars for the level held now (none on a new augment)
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

// In m_buttonMenu with the cards, so it's off during the reveal and the pick
// like they are. The menu shares the centred main layer's space, hence the offset.
void AugmentDraftPopup::addRunInfoButton() {
    float const height = kRunButtonSize;
    auto spr = augButtonSprite(CircleBaseSize::Big);
    spr->setScale(height / spr->getContentSize().height);
    spr->setCascadeOpacityEnabled(true);   // the mark is a child
    auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AugmentDraftPopup::onRunInfo));
    btn->setID("run-info-button"_spr);

    auto const win = CCDirector::get()->getWinSize();
    CCPoint const corner{ win.width - kRunButtonMargin - height / 2, win.height - kRunButtonMargin - height / 2 };
    CCPoint const origin{ (win.width - m_size.width) / 2, (win.height - m_size.height) / 2 };
    btn->setPosition(corner - origin);
    m_buttonMenu->addChild(btn);
    m_runSprite = spr;
}

void AugmentDraftPopup::onRunInfo(CCObject*) {
    if (m_picking || (m_summary && m_summary->getParent())) return;
    m_summary = summary::open();
}

void AugmentDraftPopup::visit() {
    if (m_picking) {
        // closed, already out of the scene
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

    // cards under the run summary don't hover (or play the hover sound)
    if (m_summary && !m_summary->getParent()) m_summary = nullptr;
    int hovered = -1;
    auto const mouse = getMousePos();
    for (size_t i = 0; i < m_reveal.size() && !m_summary; i++) {
        auto const p = m_reveal[i].item->convertToNodeSpace(mouse);
        auto const size = m_reveal[i].item->getContentSize();
        if (p.x >= 0.f && p.y >= 0.f && p.x <= size.width && p.y <= size.height) {
            hovered = static_cast<int>(i);
            break;
        }
    }
    if (hovered != m_hovered) {
        if (hovered >= 0) sfx::play(sfx::Cue::CardHover);
        m_hovered = hovered;
    }

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

    // goes through when the send-off ends (finishPick)
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

    // the chosen card's outline, growing and fading
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

    this->setOpacity(static_cast<GLubyte>(kOverlayOpacity * (1.f - out)));
    if (m_title) m_title->setOpacity(static_cast<GLubyte>(255.f * (1.f - out)));
    if (m_runSprite) m_runSprite->setOpacity(static_cast<GLubyte>(255.f * (1.f - out)));
    return false;
}

void AugmentDraftPopup::finishPick() {
    // Called from visit() mid scene walk and onClose removes us, so stay alive
    // until the autorelease pool drains. Close before the callback: it may
    // open the next draft or resume the level.
    this->retain();
    this->autorelease();
    auto id = m_choices[m_picked]->id;
    auto cb = std::move(m_onPick);
    Popup::onClose(nullptr);
    if (cb) cb(id);
}

} // namespace augment

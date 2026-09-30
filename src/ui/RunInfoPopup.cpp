#include "RunInfoPopup.hpp"
#include "AugmentInfoPopup.hpp"
#include "CardStyle.hpp"
#include "Fonts.hpp"
#include "../game/Language.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <climits>
#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {
    // card size = the ring's outer edge, paddings are measured from there
    constexpr float kCardWidth = 460.f;
    // screen is 320 tall and the close button sits on the card's corner
    constexpr float kMaxCardHeight = 292.f;
    constexpr float kPad = 14.f;

    // stat chips
    constexpr float kStatWidth = 112.f;
    constexpr float kStatHeight = 30.f;
    constexpr float kStatGap = 12.f;
    constexpr float kStatLabelScale = 0.38f;
    constexpr float kStatValueScale = 0.48f;
    constexpr float kStatLineGap = 0.f;
    constexpr float kStatRim = 1.6f;
    constexpr float kStatRing = 2.f;
    constexpr float kStatShadow = 3.f;

    constexpr float kSectionGap = 8.f;    // chips -> header
    constexpr float kSectionHeight = 12.f;
    constexpr float kSectionScale = 0.5f;
    constexpr float kSectionLineGap = 8.f;
    constexpr float kGridTopGap = 6.f;    // header -> grid
    constexpr float kGridGap = 8.f;
    constexpr float kEmptyHeight = 40.f;
    constexpr float kEmptyScale = 0.6f;

    // tile at full size: name, art, level + stars. Four in a row fill the
    // card's width exactly.
    constexpr float kTileRim = 2.f;
    constexpr float kTileRing = 2.5f;
    constexpr float kTileShadow = 5.f;
    constexpr float kTileWidth = 132.f;
    constexpr float kTileHeight = 109.f;
    constexpr float kTilePad = 7.f;       // top and bottom
    constexpr float kTilePadX = 14.f;     // left and right
    constexpr float kTileNameHeight = 15.f;
    constexpr float kTileNameScale = 0.55f;
    constexpr float kTileNameGap = 4.f;
    constexpr float kImageWidth = kTileWidth - 2 * kTilePadX;
    constexpr float kImageHeight = 61.f;  // 104 x 61 ~ 120:70
    constexpr float kImageRadius = 4.5f;
    constexpr float kImageBorder = 2.f;
    constexpr float kFooterHeight = 10.f;
    constexpr float kFooterScale = 0.45f;
    constexpr float kPipRadius = 3.8f;    // outline included
    constexpr float kPipSpacing = 7.f;
    constexpr float kHoverScale = 1.06f;
    constexpr float kHoverRate = 18.f;    // per second

    constexpr ccColor3B kGold = { 255, 215, 60 };

    // card top -> grid top
    constexpr float kHeaderHeight = kPad + kStatHeight + kSectionGap + kSectionHeight + kGridTopGap;

    struct Grid {
        int cols = 1;
        int rows = 1;
        float scale = 0.f;
    };

    // Column count with the biggest tiles (capped at full size). Ties go to
    // the fuller grid, so four tiles make 2x2 rather than 3 + 1.
    Grid fitGrid(int n, float width, float height) {
        Grid best;
        int bestEmpty = INT_MAX;
        for (int cols = 1; cols <= n; cols++) {
            int rows = (n + cols - 1) / cols;
            float sw = (width - (cols - 1) * kGridGap) / (cols * kTileWidth);
            float sh = (height - (rows - 1) * kGridGap) / (rows * kTileHeight);
            float scale = std::min({ 1.f, sw, sh });
            int empty = cols * rows - n;
            bool bigger = scale > best.scale + 1e-3f;
            bool tie = std::abs(scale - best.scale) <= 1e-3f;
            if (bigger || (tie && empty < bestEmpty)) {
                best = { cols, rows, scale };
                bestEmpty = empty;
            }
        }
        return best;
    }
}

RunInfoPopup* RunInfoPopup::create(Info info) {
    auto ret = new RunInfoPopup();
    if (ret->init(std::move(info))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool RunInfoPopup::init(Info info) {
    int const n = static_cast<int>(info.augments.size());
    float const inner = kCardWidth - 2 * kPad;
    Grid const grid = n > 0 ? fitGrid(n, inner, kMaxCardHeight - kHeaderHeight - kPad) : Grid{};
    float const tileW = kTileWidth * grid.scale;
    float const tileH = kTileHeight * grid.scale;
    float const gridH = n > 0 ? grid.rows * tileH + (grid.rows - 1) * kGridGap : kEmptyHeight;
    float const cardH = kHeaderHeight + gridH + kPad;

    if (!Popup::init(kCardWidth, cardH)) return false;
    this->setID("run-info-popup"_spr);
    // no brown box, just the card over a darker overlay
    m_bgSprite->setVisible(false);
    this->setOpacity(160);

    float const cx = kCardWidth / 2;
    auto panel = card::framedPanel({ kCardWidth, cardH });
    panel->setPosition({ cx, cardH / 2 });
    m_mainLayer->addChild(panel, 0);

    struct Stat {
        char const* label;
        std::string value;
        ccColor3B color;
    };
    Stat const stats[] = {
        { "Deaths", fmt::format("{}", info.deaths), ccWHITE },
        { "Session Best", fmt::format("{}%", info.sessionBest), ccWHITE },
        { "Total Best", fmt::format("{}%", info.record), kGold },
    };
    float const statsLeft = cx - (3 * kStatWidth + 2 * kStatGap) / 2;
    for (int i = 0; i < 3; i++) {
        auto chip = this->createStat(stats[i].label, stats[i].value, stats[i].color, kStatWidth);
        chip->setPosition({ statsLeft + kStatWidth / 2 + i * (kStatWidth + kStatGap), cardH - kPad - kStatHeight / 2 });
        m_mainLayer->addChild(chip, 1);
    }

    float const sectionY = cardH - kPad - kStatHeight - kSectionGap - kSectionHeight / 2;
    auto section = CCLabelBMFont::create(fmt::format("{} ({})", tr("Augments held", "보유 증강"), n).c_str(), fonts::Text);
    section->setScale(kSectionScale);
    section->setAnchorPoint({ 0.f, 0.5f });
    section->setPosition({ kPad, sectionY });
    m_mainLayer->addChild(section, 1);
    // engraved rule
    float const ruleX = kPad + section->getScaledContentSize().width + kSectionLineGap;
    float const ruleW = kCardWidth - kPad - ruleX;
    if (ruleW > 0.f) {
        auto dark = CCLayerColor::create({ 0, 0, 0, 70 }, ruleW, 1.f);
        dark->setPosition({ ruleX, sectionY });
        m_mainLayer->addChild(dark, 1);
        auto light = CCLayerColor::create({ 255, 255, 255, 45 }, ruleW, 1.f);
        light->setPosition({ ruleX, sectionY - 1.f });
        m_mainLayer->addChild(light, 1);
    }

    float const gridTop = cardH - kHeaderHeight;
    if (n == 0) {
        auto none = CCLabelBMFont::create(tr("No augments yet.", "아직 획득한 증강이 없습니다."), fonts::Text);
        none->setScale(kEmptyScale);
        none->setPosition({ cx, gridTop - kEmptyHeight / 2 });
        m_mainLayer->addChild(none, 1);
    }
    m_held = info.augments;
    for (int i = 0; i < n; i++) {
        int const row = i / grid.cols;
        int const col = i % grid.cols;
        // centre every row, a short last one too
        int const inRow = std::min(grid.cols, n - row * grid.cols);
        float const rowW = inRow * tileW + (inRow - 1) * kGridGap;

        // holder keeps the hit area fixed, visual is what the hover scales
        auto holder = CCNode::create();
        holder->setContentSize({ tileW, tileH });
        auto visual = this->createTile(m_held[i]);
        visual->setScale(grid.scale);
        visual->setPosition({ tileW / 2, tileH / 2 });
        holder->addChild(visual, 0);

        auto item = CCMenuItemSpriteExtra::create(holder, this, menu_selector(RunInfoPopup::onTile));
        item->m_scaleMultiplier = 1.04f;
        item->setTag(i);
        item->setID(fmt::format("tile-{}", m_held[i].def->id));
        item->setPosition({
            cx - rowW / 2 + tileW / 2 + col * (tileW + kGridGap),
            gridTop - tileH / 2 - row * (tileH + kGridGap)
        });
        m_buttonMenu->addChild(item);
        m_tiles.push_back({ item, visual, grid.scale, 1.f });
    }
    m_lastVisit = std::chrono::steady_clock::now();
    return true;
}

CCNode* RunInfoPopup::createStat(char const* label, std::string const& value, ccColor3B valueColor, float width) {
    auto chip = CCNode::create();
    chip->setContentSize({ width, kStatHeight });
    chip->setAnchorPoint({ 0.5f, 0.5f });

    auto box = card::gdPanel({ width, kStatHeight }, kStatRim, kStatRing, kStatShadow);
    box->setPosition({ width / 2, kStatHeight / 2 });
    chip->addChild(box, 0);

    auto caption = CCLabelBMFont::create(label, fonts::Text);
    caption->setScale(kStatLabelScale);
    caption->setColor(valueColor);
    auto number = CCLabelBMFont::create(value.c_str(), fonts::Name);
    number->limitLabelWidth(width - 2 * kTilePad, kStatValueScale, 0.3f);
    number->setColor(valueColor);

    // centre the pair by their real heights
    float const captionH = caption->getScaledContentSize().height;
    float const numberH = number->getScaledContentSize().height;
    float const top = (kStatHeight + captionH + kStatLineGap + numberH) / 2;
    caption->setPosition({ width / 2, top - captionH / 2 });
    number->setPosition({ width / 2, top - captionH - kStatLineGap - numberH / 2 });
    chip->addChild(caption, 1);
    chip->addChild(number, 1);
    return chip;
}

CCNode* RunInfoPopup::createTile(Held const& held) {
    auto const& def = *held.def;
    auto tile = CCNode::create();
    tile->setContentSize({ kTileWidth, kTileHeight });
    tile->setAnchorPoint({ 0.5f, 0.5f });

    auto box = card::gdPanel({ kTileWidth, kTileHeight }, kTileRim, kTileRing, kTileShadow);
    box->setPosition({ kTileWidth / 2, kTileHeight / 2 });
    tile->addChild(box, 0);

    float y = kTileHeight - kTilePad;
    auto name = CCLabelBMFont::create(def.name.in(language()).c_str(), fonts::Name);
    name->limitLabelWidth(kTileWidth - 2 * kTilePadX, kTileNameScale, 0.3f);
    name->setPosition({ kTileWidth / 2, y - kTileNameHeight / 2 });
    tile->addChild(name, 1);
    y -= kTileNameHeight + kTileNameGap;

    auto image = card::artSlot(def.id, { kImageWidth, kImageHeight }, kImageRadius, kImageBorder);
    image->setPosition({ kTileWidth / 2, y - kImageHeight / 2 });
    tile->addChild(image, 1);

    float const footerY = kTilePad + kFooterHeight / 2;
    auto level = CCLabelBMFont::create(fmt::format("Lv {}", held.level).c_str(), fonts::Text);
    level->setScale(kFooterScale);
    if (held.level >= def.maxLevel) level->setColor(kGold);
    level->setAnchorPoint({ 0.f, 0.5f });
    level->setPosition({ kTilePadX, footerY });
    tile->addChild(level, 1);

    auto pips = card::stars(held.level, def.maxLevel, kPipRadius, kPipSpacing);
    pips->setAnchorPoint({ 1.f, 0.5f });
    pips->setPosition({ kTileWidth - kTilePadX, footerY });
    tile->addChild(pips, 1);
    return tile;
}

void RunInfoPopup::visit() {
    int hovered = -1;
    bool const detailOpen = m_detail && m_detail->getParent();
    if (!detailOpen) {
        auto const mouse = getMousePos();
        for (size_t i = 0; i < m_tiles.size(); i++) {
            auto item = m_tiles[i].item;
            auto const p = item->convertToNodeSpace(mouse);
            auto const size = item->getContentSize();
            if (p.x >= 0.f && p.y >= 0.f && p.x <= size.width && p.y <= size.height) {
                hovered = static_cast<int>(i);
                break;
            }
        }
    }
    this->setHovered(hovered);

    // real clock, and an exponential ease so it looks the same at any fps
    auto const now = std::chrono::steady_clock::now();
    float const dt = std::min(0.1f, std::chrono::duration<float>(now - m_lastVisit).count());
    m_lastVisit = now;
    float const k = 1.f - std::exp(-kHoverRate * dt);
    for (size_t i = 0; i < m_tiles.size(); i++) {
        auto& tile = m_tiles[i];
        float const target = static_cast<int>(i) == m_hovered ? kHoverScale : 1.f;
        if (std::abs(tile.grow - target) < 1e-4f) continue;
        tile.grow += (target - tile.grow) * k;
        if (std::abs(tile.grow - target) < 1e-3f) tile.grow = target;
        tile.visual->setScale(tile.baseScale * tile.grow);
    }
    Popup::visit();
}

void RunInfoPopup::setHovered(int index) {
    m_hovered = index;
}

void RunInfoPopup::onTile(CCObject* sender) {
    int const index = sender->getTag();
    if (index < 0 || index >= static_cast<int>(m_held.size())) return;
    auto const& held = m_held[index];
    auto popup = AugmentInfoPopup::create(*held.def, held.level);
    if (!popup) return;
    m_detail = popup;
    popup->show();
}

} // namespace augment

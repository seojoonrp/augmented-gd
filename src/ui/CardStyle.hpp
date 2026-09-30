#pragma once

// Shared drawing for the cards, panels and popups.

#include <Geode/Geode.hpp>
#include <Geode/ui/NineSlice.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace augment::card {

constexpr float Rim = 2.f;
constexpr float RimRadius = 8.f;    // just outside GJ_button_01's ~6 pt corners
constexpr float Ring = 2.5f;        // GJ_button_01's black ring at sd
constexpr float SquareRadius = 9.f;  // square02b_001's corners at scale 1 (~36 of 320 uhd px)

// slices scaled so the corners come out at `radius` whatever the size
inline geode::NineSlice* roundedBox(cocos2d::CCSize size, cocos2d::ccColor3B color, float radius, GLubyte opacity = 255) {
    float const scale = radius / SquareRadius;
    auto box = geode::NineSlice::create("square02b_001.png");
    box->setScale(scale);
    box->setContentSize(size / scale);
    box->setColor(color);
    box->setOpacity(opacity);
    return box;
}

// Black border, white panel, <id>.png fitted inside. No file = empty panel.
inline cocos2d::CCNode* artSlot(std::string const& id, cocos2d::CCSize size, float radius, float border) {
    auto node = cocos2d::CCNode::create();
    node->setContentSize(size);
    node->setAnchorPoint({ 0.5f, 0.5f });
    cocos2d::CCPoint const centre{ size.width / 2, size.height / 2 };
    auto frame = roundedBox(size, { 0, 0, 0 }, radius);
    frame->setPosition(centre);
    node->addChild(frame, 0);
    cocos2d::CCSize const panel{ size.width - 2 * border, size.height - 2 * border };
    auto fill = roundedBox(panel, { 255, 255, 255 }, radius - border);
    fill->setPosition(centre);
    node->addChild(fill, 1);

    auto artName = geode::Mod::get()->expandSpriteName(fmt::format("{}.png", id));
    if (auto art = cocos2d::CCSprite::create(artName.c_str())) {
        auto artSize = art->getContentSize();
        if (artSize.width > 0.f && artSize.height > 0.f) {
            art->setScale(std::min(panel.width / artSize.width, panel.height / artSize.height));
        }
        art->setPosition(centre);
        node->addChild(art, 2);
    }
    else {
        geode::log::warn("No card art for '{}'", id);
    }
    return node;
}

// ---------------------------------------------------------------- drawn shapes
// CCDrawNode colours are premultiplied. A border in the fill colour hides the
// AA seams between strips; keep pieces fat or the mitred corners spike.

inline cocos2d::ccColor4F premul(cocos2d::ccColor3B c, float alpha = 1.f) {
    return { c.r / 255.f * alpha, c.g / 255.f * alpha, c.b / 255.f * alpha, alpha };
}

// counter-clockwise and convex, so drawPolygon's fan fill works
inline std::vector<cocos2d::CCPoint> roundedRectPoints(cocos2d::CCRect rect, float radius, int segments = 6) {
    float const r = std::min({ radius, rect.size.width / 2, rect.size.height / 2 });
    float const minX = rect.getMinX() + r, maxX = rect.getMaxX() - r;
    float const minY = rect.getMinY() + r, maxY = rect.getMaxY() - r;
    cocos2d::CCPoint const centres[4] = { { maxX, maxY }, { minX, maxY }, { minX, minY }, { maxX, minY } };
    std::vector<cocos2d::CCPoint> points;
    for (int corner = 0; corner < 4; corner++) {
        for (int i = 0; i <= segments; i++) {
            float const a = (corner + static_cast<float>(i) / segments) * 1.5707963f;
            points.push_back({ centres[corner].x + r * std::cos(a), centres[corner].y + r * std::sin(a) });
        }
    }
    return points;
}

// GD menu tile, after CreatorLayer's buttons. The gradient bands have square
// corners, the ring hides them. `size` = the ring's outer edge.
inline cocos2d::CCNode* gdPanel(cocos2d::CCSize size, float rim, float ring, float shadowOffset) {
    constexpr cocos2d::ccColor4B kTopLeft = { 200, 254, 89, 255 };
    constexpr cocos2d::ccColor4B kTopRight = { 107, 208, 19, 255 };
    constexpr cocos2d::ccColor4B kLowLeft = { 150, 252, 62, 255 };
    constexpr cocos2d::ccColor4B kLowRight = { 70, 162, 13, 255 };
    constexpr cocos2d::ccColor4B kStripLeft = { 75, 127, 30, 255 };
    constexpr cocos2d::ccColor4B kStripRight = { 38, 84, 9, 255 };
    constexpr float kTopShare = 0.49f;      // of the body's height
    constexpr float kStripShare = 0.045f;
    constexpr float kMinStrip = 2.f;
    constexpr GLubyte kShadowOpacity = 102; // 40%

    auto node = cocos2d::CCNode::create();
    node->setContentSize(size);
    node->setAnchorPoint({ 0.5f, 0.5f });
    cocos2d::CCPoint const centre{ size.width / 2, size.height / 2 };
    cocos2d::CCSize const outer{ size.width + 2 * rim, size.height + 2 * rim };
    float const outerRadius = ring + rim;   // keeps the white rim even round the corner

    auto shadow = roundedBox(outer, { 0, 0, 0 }, outerRadius, kShadowOpacity);
    shadow->setPosition(centre + cocos2d::CCPoint{ shadowOffset, -shadowOffset });
    node->addChild(shadow, 0);
    auto white = roundedBox(outer, { 255, 255, 255 }, outerRadius);
    white->setPosition(centre);
    node->addChild(white, 1);
    auto black = roundedBox(size, { 0, 0, 0 }, ring);
    black->setPosition(centre);
    node->addChild(black, 2);

    float const bodyW = size.width - 2 * ring;
    float const bodyH = size.height - 2 * ring;
    float const stripH = std::max(kMinStrip, bodyH * kStripShare);
    float const topH = bodyH * kTopShare;
    float const lowH = bodyH - topH - stripH;
    auto band = [&](cocos2d::ccColor4B left, cocos2d::ccColor4B right, float y, float h) {
        auto layer = cocos2d::CCLayerGradient::create(left, right, { 1.f, 0.f });
        layer->setContentSize({ bodyW, h });
        layer->setPosition({ ring, ring + y });
        node->addChild(layer, 3);
    };
    band(kStripLeft, kStripRight, 0.f, stripH);
    band(kLowLeft, kLowRight, stripH, lowH);
    band(kTopLeft, kTopRight, stripH + lowH, topH);
    return node;
}

// vertical gradient as horizontal strips that follow the corner arcs
// (1 pt strips in the corners, 3 pt between)
inline cocos2d::CCDrawNode* gradientBox(cocos2d::CCSize size, cocos2d::ccColor3B top, cocos2d::ccColor3B bottom, float radius) {
    auto node = cocos2d::CCDrawNode::create();
    node->setContentSize(size);
    node->setAnchorPoint({ 0.5f, 0.5f });
    float const w = size.width, h = size.height;
    float const r = std::min({ radius, w / 2, h / 2 });
    auto inset = [&](float y) {
        float const dy = y < r ? r - y : (y > h - r ? y - (h - r) : 0.f);
        return r - std::sqrt(std::max(0.f, r * r - dy * dy));
    };
    std::vector<float> ys;
    for (float y = 0.f; y < r; y += 1.f) ys.push_back(y);
    for (float y = r; y < h - r; y += 3.f) ys.push_back(y);
    for (float y = h - r; y < h; y += 1.f) ys.push_back(y);
    ys.push_back(h);
    auto mix = [](GLubyte a, GLubyte b, float t) {
        return static_cast<GLubyte>(std::lround(a + (b - a) * t));
    };
    for (size_t i = 0; i + 1 < ys.size(); i++) {
        float const y0 = ys[i], y1 = ys[i + 1];
        if (y1 <= y0) continue;
        float const t = (y0 + y1) / 2 / h;
        auto colour = premul({ mix(bottom.r, top.r, t), mix(bottom.g, top.g, t), mix(bottom.b, top.b, t) });
        float const a0 = inset(y0), a1 = inset(y1);
        cocos2d::CCPoint quad[4] = { { a0, y0 }, { w - a0, y0 }, { w - a1, y1 }, { a1, y1 } };
        node->drawPolygon(quad, 4, colour, 0.3f, colour);
    }
    return node;
}

// Level stars. A sprite, since a CCDrawNode star spiked at the tips.
// `radius` includes the outline; the caller sets the anchor.
inline cocos2d::CCNode* stars(int filled, int total, float radius, float spacing) {
    constexpr float kStarTilt = -12.f;   // degrees, positive is clockwise in cocos
    auto node = cocos2d::CCNode::create();
    float const d = 2 * radius;
    node->setContentSize({ (total - 1) * spacing + d, d });
    for (int i = 0; i < total; i++) {
        auto star = cocos2d::CCSprite::create("round-star.png"_spr);
        if (!star) {
            geode::log::warn("Stars: no round-star.png sprite");
            break;
        }
        star->setScale(d / star->getContentSize().width);
        star->setRotation(kStarTilt);
        star->setColor(i < filled ? cocos2d::ccColor3B{ 255, 215, 60 } : cocos2d::ccColor3B{ 115, 115, 115 });
        star->setPosition({ radius + i * spacing, radius });
        node->addChild(star);
    }
    return node;
}

// Popup card: the draft cards' rim and ring around CreatorLayer's blue
// gradient (stops short of its darkest). `size` = the ring's outer edge.
inline cocos2d::CCNode* framedPanel(cocos2d::CCSize size) {
    constexpr float kRingRadius = 6.5f;
    constexpr cocos2d::ccColor3B kTop = { 0, 96, 241 };
    constexpr cocos2d::ccColor3B kBottom = { 0, 56, 142 };

    auto node = cocos2d::CCNode::create();
    node->setContentSize(size);
    node->setAnchorPoint({ 0.5f, 0.5f });
    cocos2d::CCPoint const centre{ size.width / 2, size.height / 2 };

    auto shadow = roundedBox({ size.width + 2 * Rim + 4, size.height + 2 * Rim + 4 }, { 0, 0, 0 }, RimRadius + 2, 90);
    shadow->setPosition(centre + cocos2d::CCPoint{ 0.f, -4.f });
    node->addChild(shadow, 0);
    auto rim = roundedBox({ size.width + 2 * Rim, size.height + 2 * Rim }, { 255, 255, 255 }, RimRadius);
    rim->setPosition(centre);
    node->addChild(rim, 1);
    auto ring = roundedBox(size, { 0, 0, 0 }, kRingRadius);
    ring->setPosition(centre);
    node->addChild(ring, 2);

    cocos2d::CCSize const body{ size.width - 2 * Ring, size.height - 2 * Ring };
    auto fill = gradientBox(body, kTop, kBottom, kRingRadius - Ring);
    fill->setPosition(centre);
    node->addChild(fill, 3);
    return node;
}

} // namespace augment::card

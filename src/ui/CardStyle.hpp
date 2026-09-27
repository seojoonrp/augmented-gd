#pragma once

// The GD-button card look shared by the draft cards and the AUG resume
// popup: a white rim around GJ_button_01 (black ring + flat green).

#include <Geode/Geode.hpp>
#include <Geode/ui/NineSlice.hpp>

namespace augment::card {

constexpr float Rim = 2.f;
constexpr float RimRadius = 8.f;    // just outside GJ_button_01's ~6 pt corners
constexpr float Ring = 2.5f;        // GJ_button_01's black ring at sd
// square02b_001's corner radius at scale 1 (measured: ~36 of 320 uhd px).
constexpr float SquareRadius = 9.f;

// square02b_001 is a plain white rounded square; the slices are scaled so
// the corner radius comes out as asked, whatever the box size.
inline geode::NineSlice* roundedBox(cocos2d::CCSize size, cocos2d::ccColor3B color, float radius, GLubyte opacity = 255) {
    float const scale = radius / SquareRadius;
    auto box = geode::NineSlice::create("square02b_001.png");
    box->setScale(scale);
    box->setContentSize(size / scale);
    box->setColor(color);
    box->setOpacity(opacity);
    return box;
}

// Rim + green body, `size` = the body; origin bottom-left of the body.
inline cocos2d::CCNode* panel(cocos2d::CCSize size) {
    auto node = cocos2d::CCNode::create();
    node->setContentSize(size);
    node->setAnchorPoint({ 0.5f, 0.5f });
    cocos2d::CCPoint const centre{ size.width / 2, size.height / 2 };
    auto rim = roundedBox({ size.width + 2 * Rim, size.height + 2 * Rim }, { 255, 255, 255 }, RimRadius);
    rim->setPosition(centre);
    node->addChild(rim, 0);
    auto body = geode::NineSlice::create("GJ_button_01.png");
    body->setContentSize(size);
    body->setPosition(centre);
    node->addChild(body, 1);
    return node;
}

} // namespace augment::card

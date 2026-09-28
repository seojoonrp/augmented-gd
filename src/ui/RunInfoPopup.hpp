#pragma once

#include "../core/AugmentDef.hpp"

#include <Geode/ui/Popup.hpp>
#include <Geode/utils/cocos.hpp>

#include <chrono>
#include <string>
#include <vector>

class CCMenuItemSpriteExtra;

namespace augment {

// Opened from the pause menu of a run level: this run's deaths and best and
// every run's record on the level (Records.hpp) in a row of stat chips, then
// the augments held so far as a grid of tiles — card art, name, level — in
// GD's own menu colours (blue card, green panels: CardStyle.hpp). The grid
// picks the column count that gives the biggest tiles for however many
// augments there are, so it never needs to scroll. A tile grows a little
// under the mouse and a click opens that augment's card (AugmentInfoPopup).
// The close button or Esc goes back to the pause menu.
class RunInfoPopup : public geode::Popup {
public:
    struct Held {
        AugmentDef const* def = nullptr;
        int level = 0;
    };
    struct Info {
        int deaths = 0;
        int sessionBest = 0;   // whole percent, this run
        int record = 0;        // whole percent, every run on the level
        std::vector<Held> augments;   // table order
    };

    static RunInfoPopup* create(Info info);

protected:
    bool init(Info info);
    cocos2d::CCNode* createStat(char const* label, std::string const& value, cocos2d::ccColor3B valueColor, float width);
    // Full size (kTileWidth x kTileHeight); the grid scales it.
    cocos2d::CCNode* createTile(Held const& held);
    // Hover is polled here: cocos has no mouse-over event, so each frame the
    // mouse is tested against every tile (death-tracker's GraphPoint does the
    // same, `refs/death-tracker/src/nodes/GraphPoint.cpp:35-46`).
    void visit() override;
    void setHovered(int index);
    void onTile(cocos2d::CCObject* sender);

    struct Tile {
        CCMenuItemSpriteExtra* item = nullptr;
        cocos2d::CCNode* visual = nullptr;  // the tile drawing, scaled on hover
        float baseScale = 1.f;              // the grid's fit
        float grow = 1.f;                   // hover factor on top, eased
    };
    std::vector<Held> m_held;
    std::vector<Tile> m_tiles;
    int m_hovered = -1;
    std::chrono::steady_clock::time_point m_lastVisit;
    // The detail card while it is open; no hover underneath it.
    geode::Ref<geode::Popup> m_detail;
};

} // namespace augment

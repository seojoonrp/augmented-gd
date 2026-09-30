#pragma once

#include "../core/AugmentDef.hpp"

#include <Geode/ui/Popup.hpp>
#include <Geode/utils/cocos.hpp>

#include <chrono>
#include <string>
#include <vector>

class CCMenuItemSpriteExtra;

namespace augment {

// Run summary from the pause menu: stat chips, then the held augments as a
// grid sized so it never has to scroll. Click a tile for its card.
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
    // full size, the grid scales it
    cocos2d::CCNode* createTile(Held const& held);
    // hover is polled every frame, cocos has no mouse-over event
    // (same trick as death-tracker's graph points)
    void visit() override;
    void setHovered(int index);
    void onTile(cocos2d::CCObject* sender);

    struct Tile {
        CCMenuItemSpriteExtra* item = nullptr;
        cocos2d::CCNode* visual = nullptr;  // scaled on hover
        float baseScale = 1.f;              // grid fit
        float grow = 1.f;                   // hover factor on top
    };
    std::vector<Held> m_held;
    std::vector<Tile> m_tiles;
    int m_hovered = -1;
    std::chrono::steady_clock::time_point m_lastVisit;
    // no hover while the detail card is up
    geode::Ref<geode::Popup> m_detail;
};

} // namespace augment

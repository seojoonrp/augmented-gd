#pragma once

// Run summary popup, opened from the pause menu and from the draft popup
// (GD hides the cursor in levels, so a HUD button can't be clicked).

namespace cocos2d { class CCNode; }

namespace augment::summary {

// the popup, or null without a live session
cocos2d::CCNode* open();

} // namespace augment::summary

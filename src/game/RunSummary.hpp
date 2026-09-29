#pragma once

// Opening the run summary (RunInfoPopup) for the live session: the pause
// menu's run button. (A HUD button that paused the game for it was tried and
// dropped on 2026-09-29: GD hides the cursor in levels, so it could not be
// clicked.)

namespace augment::summary {

// Over whatever is on screen (the pause menu).
void open();

} // namespace augment::summary

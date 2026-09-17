#pragma once

// The draft popup flow in a level: take one pending draft, roll cards, pause
// the game (director), show the cursor, and chain the next popup after a
// pick while any drafts remain. Nothing here captures a PlayLayer: the pick
// callback talks to AugmentManager and its session at call time.

namespace augment::draft {

// Shows the next pending draft (one is consumed). No-op when nothing is
// pending or nothing is left to draft (pending drafts are then dropped).
void showNext();
// A draft popup is up and the director is paused for it.
bool isOpen();
// Resume the game without touching the cursor. Safe when nothing is open,
// so PlayLayer::onQuit / run start / run end all call it.
void abandon();

} // namespace augment::draft

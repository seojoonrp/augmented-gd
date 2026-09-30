#pragma once

// Draft popups in a level: pause the director, show the cursor, chain popups
// while drafts are pending. Never holds a PlayLayer.

namespace augment::draft {

// consumes one pending draft; if nothing is left to draft, drops the rest
void showNext();
bool isOpen();
// resumes without touching the cursor; safe to call when nothing is open
void abandon();

} // namespace augment::draft

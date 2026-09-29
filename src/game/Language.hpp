#pragma once

// The `language` setting (mod.json, top of the settings page): which of the
// texts in core/Lang.hpp the player sees. Read at call time, so anything
// built after a change follows it; text already on screen keeps its language
// until it is shown again (the next notice, draft or popup).

#include "../core/Lang.hpp"

namespace augment {

// English unless the setting says Korean.
Lang language();

// `en` or `ko`, whichever the setting picks: for the player-facing lines
// that live next to the code that shows them (notices, popup titles).
char const* tr(char const* en, char const* ko);

} // namespace augment

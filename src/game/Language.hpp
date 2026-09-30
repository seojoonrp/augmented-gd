#pragma once

// The `language` setting, read at call time: text already on screen keeps its
// language until it's rebuilt.

#include "../core/Lang.hpp"

namespace augment {

Lang language();

// for player-facing lines outside the augment table
char const* tr(char const* en, char const* ko);

} // namespace augment

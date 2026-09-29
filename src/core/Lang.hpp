#pragma once

// The languages the mod's player-facing text comes in. src/core stores every
// text in all of them; the `language` setting picks one (game/Language.hpp).

#include <string>

namespace augment {

enum class Lang { English, Korean };

// One piece of player-facing text in each language.
struct LocalText {
    std::string en;
    std::string ko;

    std::string const& in(Lang lang) const { return lang == Lang::Korean ? ko : en; }
};

} // namespace augment

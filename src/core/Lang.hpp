#pragma once

#include <string>

namespace augment {

enum class Lang { English, Korean };

struct LocalText {
    std::string en;
    std::string ko;

    std::string const& in(Lang lang) const { return lang == Lang::Korean ? ko : en; }
};

} // namespace augment

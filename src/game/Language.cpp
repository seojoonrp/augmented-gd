#include "Language.hpp"

#include <Geode/Geode.hpp>

#include <string_view>

using namespace geode::prelude;

namespace augment {

Lang language() {
    // string_view: no copy per lookup
    return Mod::get()->getSettingValue<std::string_view>("language") == "Korean" ? Lang::Korean : Lang::English;
}

char const* tr(char const* en, char const* ko) {
    return language() == Lang::Korean ? ko : en;
}

} // namespace augment

#include "Language.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>

#include <string_view>

using namespace geode::prelude;

namespace augment {

Lang language() {
    // A view of the setting's own string: no copy per lookup.
    return Mod::get()->getSettingValue<std::string_view>("language") == "Korean" ? Lang::Korean : Lang::English;
}

char const* tr(char const* en, char const* ko) {
    return language() == Lang::Korean ? ko : en;
}

} // namespace augment

$on_mod(Loaded) {
    log::info("Language: {}", Mod::get()->getSettingValue<std::string_view>("language"));
    listenForSettingChanges<std::string_view>("language", [](std::string_view value) {
        log::info("Language -> {} (text shown from now on)", value);
    });
}

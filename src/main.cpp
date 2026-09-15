// Augment Mode: a roguelite layer on top of a single level.
// Hooks live in src/hooks/, UI in src/ui/, run state in src/core/.

#include <Geode/Geode.hpp>

using namespace geode::prelude;

$on_mod(Loaded) {
    log::info("Augment Mode loaded");
}

#pragma once

// Factories for every augment behaviour, one per file in this directory.
// LevelSession creates them in table order (docs/DESIGN.md).

#include "Augment.hpp"

#include <memory>
#include <vector>

namespace augment {

std::unique_ptr<Augment> makeShield();
std::unique_ptr<Augment> makeSlowMo();
std::unique_ptr<Augment> makeStartPos();
std::unique_ptr<Augment> makeForesight();
std::unique_ptr<Augment> makeUnmirror();
std::unique_ptr<Augment> makeHitboxScales();   // hazard-hitbox + wave-hitbox + nerve
std::unique_ptr<Augment> makeDraftCount();
std::unique_ptr<Augment> makeCat();
std::unique_ptr<Augment> makeBrake();

// Table order, so HUD rows and grant dispatch follow docs/DESIGN.md.
std::vector<std::unique_ptr<Augment>> makeAllAugments();

} // namespace augment

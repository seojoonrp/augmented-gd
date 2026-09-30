#pragma once

// One factory per augment file.

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
std::unique_ptr<Augment> makeMissile();
std::unique_ptr<Augment> makeBerserk();

// table order (HUD rows and grant dispatch follow it)
std::vector<std::unique_ptr<Augment>> makeAllAugments();

} // namespace augment

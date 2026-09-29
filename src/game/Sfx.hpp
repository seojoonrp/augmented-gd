#pragma once

// The mod's sound effects. Played through FMOD directly, the way xdBot's
// clickbot plays its clicks (refs/xdbot/src/hacks/clickbot.cpp:93-160): GD's
// own FMODAudioEngine::playEffect loads a file it has not played before
// asynchronously and starts it from its scheduled update, which a draft's
// director pause stops, so the draft's own sounds would wait for the pick.
// Volume follows GD's SFX volume; the `sound-effects` setting turns them off.

namespace augment::sfx {

enum class Cue {
    CardHover,       // the mouse moves onto a draft card
    CardPick,        // a draft card is picked
    MissileImpact,   // a missile lands (the blast)
};

void play(Cue cue);

} // namespace augment::sfx

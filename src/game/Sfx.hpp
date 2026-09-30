#pragma once

// Played through FMOD directly, like xdBot's clickbot. GD's playEffect loads
// new files async and starts them from its scheduled update, which the draft's
// director pause stops, so draft sounds would wait for the pick.

namespace augment::sfx {

enum class Cue {
    CardHover,
    CardPick,
    MissileImpact,
};

void play(Cue cue);

} // namespace augment::sfx

#pragma once

// The three global scales the hot hooks read every frame: game time
// (CCScheduler), hazard hitboxes and the player's wave hitbox (GameObject).
// Globals because the readers are hot virtuals that run for every object in
// the game; the level session owns them and every setter logs on change.
// The readers are inline so the hooks pay a load, not a call.

namespace augment::scales {

// Time has two layers: the base (slow-mo) and an override that wins while
// set (brake). g_time is the effective product the hook reads.
inline float g_timeBase = 1.f;
inline float g_timeOverride = 0.f;   // 0 = none
inline float g_time = 1.f;
inline float g_hazard = 1.f;
inline float g_wave = 1.f;

inline float time() { return g_time; }
inline float hazard() { return g_hazard; }
inline float wave() { return g_wave; }

// Scales the scheduler's dt (physics, actions, our timers) and matches the
// FMOD master pitch so the music stays in sync. setTime is the base speed
// (slow-mo); setTimeOverride replaces it while non-zero (brake), 0 clears.
void setTime(float scale);
void setTimeOverride(float scale);
// Both time layers back to normal speed.
void resetTime();
// 1.0 = untouched.
void setHazard(float scale);
void setWave(float scale);
// Everything back to 1.0; safe to call from PlayLayer::onQuit.
void resetAll();

} // namespace augment::scales

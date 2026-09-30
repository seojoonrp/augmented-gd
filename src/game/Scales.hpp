#pragma once

// Globals read by hot hooks (scheduler dt, hazard and wave hitboxes). Inline
// getters so the per-object hooks pay a load, not a call.

namespace augment::scales {

// time: base (slow-mo) plus an override that wins while set (brake).
// g_time is what the hook reads.
inline float g_timeBase = 1.f;
inline float g_timeOverride = 0.f;   // 0 = none
inline float g_time = 1.f;
inline float g_hazard = 1.f;
inline float g_wave = 1.f;

inline float time() { return g_time; }
inline float hazard() { return g_hazard; }
inline float wave() { return g_wave; }

// scales the scheduler's dt and the FMOD master pitch, so the music stays in
// sync. override 0 clears it
void setTime(float scale);
void setTimeOverride(float scale);
void resetTime();
// 1.0 = untouched
void setHazard(float scale);
void setWave(float scale);
// safe from PlayLayer::onQuit
void resetAll();

} // namespace augment::scales

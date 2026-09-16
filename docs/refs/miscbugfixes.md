# miscbugfixes — Cvolton/miscbugfixes-geode

Pinned `6ffab7b` (2026-08-06), Geode 5.8.2, GD 2.2081. One small file per
fix; the cleanest examples of minimal `$modify` classes and of
`STATIC_BOOL_SETTING(var, setting-key)` (`src/_Utils.hpp`) for a cached
setting.

## Files of interest

| File | Hooks | Why it matters to us |
|---|---|---|
| `src/MirrorShowHitboxesFix.cpp` | `CCDrawNode::drawPolygon/drawCircle`, `GJBaseGameLayer::updateDebugDraw`, `GameObject::determineSlopeDirection` | GD's debug draw is reachable only by wrapping `updateDebugDraw()` with a static flag; negative `borderWidth` when mirrored; `m_bUseArea` on circles |
| `src/StartPosMirrorModeFix.cpp` | `PlayLayer::resetLevel` | calls `PlayLayer::toggleFlipped(m_startPosObject->m_startSettings->m_mirrorMode, true)` after reset — another `toggleFlipped` user |
| `src/PracticeMusicSyncPulseFix.cpp` | PlayLayer / FMOD | practice-mode music sync; read before touching `FMODAudioEngine` timing |
| `src/DualWaveTrailPlayer2Fix.cpp`, `src/MiniSwingFix.cpp` | `PlayerObject` | player-object internals (`m_isMini`, `m_vehicleSize`) |
| `src/_MenuLayerManager.cpp` | `MenuLayer::init` | once-per-launch work pattern |

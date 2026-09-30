# betterinfo — Cvolton/betterinfo-geode

Pinned `305e1e5` (2026-09-13), Geode 5.10.1, GD 2.2081. Large (149 files);
mostly level-browser/profile UI. Use as a style reference for popups, lists
and LevelInfoLayer additions, not for gameplay.

## Relevant files

| File | Hooks | Purpose |
|---|---|---|
| `src/hooks/PlayLayer.cpp:16-50` | `PlayLayer::levelComplete/init/onQuit/resetLevel` | logs attempts/completions with `m_isPracticeMode`; `onQuit` reads `m_level->m_levelType == GJLevelType::Saved` |
| `src/hooks/LevelInfoLayer.cpp` | `LevelInfoLayer::init` | reaches `creator-info-menu` / `creator-name` by ID, adds its own buttons |
| `src/layers/` | — | many `geode::Popup` subclasses and `ListView`/`ScrollLayer` uses; grep here for a UI widget before writing one |
| `src/utils/` | — | `BetterInfo::…` helpers (level type checks, formatting) |

Grep: `scripts\refgrep.ps1 "Popup<" -Ref betterinfo` for popup subclasses,
`scripts\refgrep.ps1 "setLayout" -Ref betterinfo` for layout usage.

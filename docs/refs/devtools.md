# devtools — geode-sdk/DevTools

Pinned `f36931a` (2026-08-19), Geode 5.8.2, GD 2.2081, MIT. In-game node-tree
inspector (ImGui). Install it in GD to click through a live scene when a
`getChildByID` returns null or a layout looks wrong.

## Relevant code

| File | Hooks | Why |
|---|---|---|
| `src/backend.cpp:504-540` | `$on_mod(Loaded) { KeyboardInputEvent().listen(...) }` | the *other* way to register the global key listener (mod-loaded hook instead of `$execute`); forwards to ImGui, returns `Propagate` |
| `src/backend.cpp` | `CCEGLView`, `CCDirector`, `CCTouchDispatcher`, `CCMouseDispatcher`, `CCKeyboardDispatcher` | how to sit between cocos input and GD without breaking it |
| `src/DevTools.cpp`, `src/pages/` | — | node attribute dumps: which properties matter for layout debugging |

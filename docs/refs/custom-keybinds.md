# custom-keybinds — geode-sdk/CustomKeybinds

Pinned `426a9aa` (2026-08-27), Geode 5.9.0, GD 2.2081. **Installed (v2.2.2).**
Geode Team's reference for the Geode 5 keybind-settings API: it *only* uses
`"type": "keybind"` settings and `KeybindSettingPressedEventV3`; no raw
`KeyboardInputEvent`.

## Files

| File | Hooks | Purpose |
|---|---|---|
| `src/UILayer.cpp` | `PauseLayer::customSetup/keyDown`, `UILayer::init/handleKeypress` | gameplay + pause keybinds, and the key swallowing described below |
| `src/EditorUI.cpp` | `EditorUI`, `EditorPauseLayer` | editor keybinds (same pattern) |
| `src/main.cpp` | `AppDelegate` | migration of old settings |

## Patterns worth copying

- **Node-scoped listener** (`src/UILayer.cpp:47-102`, PauseLayer):
  ```cpp
  this->addEventListener(
      KeybindSettingPressedEventV3(Mod::get(), "unpause-level"),
      [this](Keybind const& keybind, bool down, bool repeat, double timestamp) {
          auto ref = Ref(this);           // keep the node alive during the callback
          if (repeat || !down) return ListenerResult::Propagate;
          …
          return ListenerResult::Stop;
      });
  ```
  Removed automatically when the node dies → no dangling `PlayLayer*`
  (CLAUDE.md rule 5 satisfied for free). `defineKeybind` at `:321-329` does the
  same on `GJBaseGameLayer::get()` for gameplay binds.
- Returning `Stop` from the keybind listener makes the loader's
  `KeyboardInputEvent` listener return `Stop` too, so the key never reaches
  `CCKeyboardDispatcher` (see loader `LoaderImpl.cpp:440`).
- `mod.json` keybind settings: `"default"` may be a string or an array;
  `"priority"` orders settings that share a key (`mod.json` `jump-p1` has -2).
- Hook priority: `static void onModify(auto& self) { (void)self.setHookPriority("UILayer::handleKeypress", Priority::Late); }` (`:110-112`).

## Traps

- `UILayer::handleKeypress` is **fully overridden** (`:331-337`): only
  `KEY_Escape` (or a one-shot `allowKeyDownThrough`) reaches GD. Hooking
  `handleKeypress` ourselves will see nothing; `UILayer::keyDown` (one level
  up, `win 0x4cde50`) still sees every key.
- `PauseLayer::keyDown` is likewise reduced to Escape (`:104-108`).

## Solved (2026-09-16): why our hotkeys never fired

This mod's `place-checkpoint` (Z) and `delete-checkpoint` (X) settings
(`mod.json:80-92`, priority 0) get node-scoped listeners on the PlayLayer that
return `Stop` unless the level is paused (`src/UILayer.cpp:229-242`) — in
every mode, not just practice. The loader dispatches keybind settings from a
`KeyboardInputEvent` listener registered before any mod binary loads
(`LoaderImpl.cpp:409`), so it ran before ours and stopped the event. Our fix:
`"priority": -5` on our settings (dispatched first) + a raw listener at
priority -1. Full account in `docs/GD-INTERNALS.md` → "Keyboard input".

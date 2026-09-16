# node-ids — geode-sdk/NodeIDs

Pinned `fc9b3aa` (2026-08-21), Geode 5.8.2, GD 2.2081. **Installed (v1.23.3).**
Source of every `getChildByID` string. Use `scripts\nodeids.ps1 <Layer>`
instead of grepping the dll.

- One file per layer in `src/<Layer>.cpp`, registered with
  `$register_ids(Layer) { … }`; IDs are set with `setIDSafe<T>(this, index, "id")`
  or `node->setID("id")`.
- Layers we touch: `LevelInfoLayer` (`other-menu`, `settings-menu`,
  `left-side-menu`, `right-side-menu`, `back-menu`, `creator-info-menu`),
  `PlayLayer` (`ui-layer`, `progress-bar`, `percentage-label`, `hitbox-node`),
  `UILayer` (`pause-button-menu`, `checkpoint-menu`, `add-checkpoint-button`),
  `PauseLayer` (`left-button-menu`, `normal-mode-label`, …), `EndLevelLayer`.
- IDs are assigned from a hook at `Priority::VeryEarlyPost`
  (`$GEODE_SDK/loader/include/Geode/utils/NodeIDs.hpp:8`), i.e. right after
  GD's own `init` returns; a default-priority `$modify` on the same `init`
  sees them. Only a hook that itself runs at `VeryEarly` or earlier would not.

# geode-docs — geode-sdk/docs

Pinned `e7dac37` (2026-09-14), MIT. Markdown source of docs.geode-sdk.org.
`main` already contains `tutorials/migrate-v6.md`, so some pages may describe
Geode 6 API; we build against **5.10.1** — when a doc and
`$GEODE_SDK/loader/include/Geode/**` disagree, the header wins.

## Pages to read for our work

| Topic | File |
|---|---|
| `$modify`, Fields, `onModify`, calling the original | `tutorials/modify.md`, `tutorials/fields.md`, `tutorials/hookpriority.md` |
| Events, `GlobalEvent`, node-scoped `CCNode::addEventListener` | `tutorials/events.md` (`:237-253` for `addEventListener`) |
| Settings incl. `"type": "keybind"` and `listenForSettingChanges` | `mods/settings.md` (`:355` keybind) |
| `geode::Popup`, buttons, layouts, positioning, node tree | `tutorials/popup.md`, `buttons.md`, `layouts.md`, `positioning.md`, `nodetree.md` |
| Casting across DLLs (`typeinfo_cast`) | `tutorials/casting.md` |
| Manual/raw hooks | `tutorials/manualhooks.md` |
| Logging | `tutorials/logging.md` |
| What changed in Geode 5 (events rework, C++23) | `tutorials/migrate-v5.md` |
| Save data | `mods/savedata.md` (needed for run persistence) |
| GD class reference notes | `geometrydash/` |

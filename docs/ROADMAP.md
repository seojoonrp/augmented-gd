# Roadmap — goals, and what to build next by difficulty

Written 2026-09-16. Companion to `DESIGN.md` (rules) and `STATUS.md` (state).
Update when an item is built, verified, or rejected.

## 1. What the project is

**One sentence:** any GD level becomes a roguelite run — die, draft an augment,
get stronger, eventually clear.

Design pillars (implicit in `DESIGN.md`, made explicit here):

1. **Never stall.** The gauge guarantees a draft while stuck and speeds drafts
   up while progressing. Every system must keep that promise.
2. **Augments assist, they never play for you.** No autoplay, no instant
   complete, no skipping to a later start. The player still clears from 0 %.
3. **Per-run state only.** Nothing carries between runs (no meta-progression)
   until the core loop is fun on its own.
4. **Hooks, not byte patches.** Draw/scale/skip things ourselves through
   verified bindings (hitboxes were the precedent). Byte patches are the last
   resort and must be marked as such.

Augment design space (every candidate falls in one):

| Category | Purpose | Existing |
|---|---|---|
| Forgiveness | survive a mistake | Shield, Checkpoint |
| Time | more reaction time | Slow-Mo |
| Information | see what is coming | Foresight |
| Level edit | remove a specific difficulty | Unmirror, hazard-hitbox |
| Economy | more / faster drafts | — |
| Curse | downside now, bigger payoff | — |

### Milestones

| # | Milestone | State |
|---|---|---|
| M1 | Entry: AUG button, Start / Continue / Restart | done, verified |
| M2 | Gauge, draft popup, mandatory pick, HUD | done, verified |
| M3 | First 5 augments | 3 verified (Shield, Slow-Mo, Foresight); Checkpoint and Unmirror unverified |
| **M4** | **Unblock input** (hotkeys, or a no-key fallback) | **blocked** — see `GD-INTERNALS.md` |
| M5 | Pool to ~12 with Tier-1 augments | next (pool is at 9, all implemented since 2026-09-17) |
| M6 | Draft depth: rarity, curses, prerequisites, reroll | after M5 |
| M7 | Run persistence, run summary, safe mode → release candidate | — |
| M8 | Tier-2/3 augments (new GD hooks) | — |

## 2. Difficulty scale

Difficulty here means **how many unverified GD facts a feature depends on**,
because that — not code volume — is what has cost test rounds so far.

| Tier | Meaning | Expected cost |
|---|---|---|
| **T1** | Only existing hooks (`destroyPlayer`, `resetLevel`, `postUpdate`, `addObject`, draft popup) and existing infra (time scale, draw node, object index, HUD, manager). | 1 build + 1 test round |
| **T2** | One new hook on a binding that has a `win` address (not inline) and a reference-mod precedent or obvious semantics. | 1–2 rounds |
| **T3** | New hook whose semantics are unverified (camera, opacity pipeline, checkpoints at arbitrary times, save path). Expect to change approach once. | 3+ rounds |
| **T4** | Needs the input path (blocked by M4) or engine-deep work (physics, prediction). | unbounded until M4 |

**Reference caveat.** Precedents below cite OpenHack, which targets GD 2.206 /
Geode 3.4 (stale — patterns only, never offsets). `HARNESS-PLAN.md` is replacing
it with 2.2081-era refs; before building a T2/T3 item, grep the newer ones first:
QOLMod (hitboxes, noclip, speedhack, key input), xdBot (checkpoints, frame step,
slow-mo), click-between-frames (raw Windows input).

Infra facts this relies on (all in `GD-INTERNALS.md`): verified — skipping
`destroyPlayer` = noclip, scheduler dt scaling + FMOD pitch, `CCDrawNode` in
`m_objectLayer`, `getObjectRect`/`m_objectRadius`/`m_objectType`,
`getCurrentPercent`. Unverified — checkpoint respawn, mirror neutralisation,
any keyboard path.

## 3. Candidate augments

Columns: levels · category · effect · what it reuses / needs · risk.

### T1 — existing infra only

| id | name | lv | cat | effect | reuses / needs | risk / note |
|---|---|---|---|---|---|---|
| `reflex` | Reflex | 3 | time | Game runs at 80/70/60 % while a hazard is within 60/90/120 units ahead of the player. | object index (sorted `objectsByX`, currently built only by Foresight → move into a shared `ensureObjectIndex()`), `setGameSpeed`. Combine with Slow-Mo by taking the lower scale. | Pitch flicker when hazards are dense → hysteresis (stay slow ≥ 0.3 s). |
| `deathmarks` | Death Marks | 2 | info | Lv1: an X drawn in the world at every death position of this run. Lv2: ticks on the progress bar too. | draw node; manager stores `(x, y, percent)` per death. Progress-bar ticks: `PlayLayer::m_progressBar` (CCSprite, verified in bindings) + our own child sprites. | Lv2 bar geometry is cocos-only but untested. |
| `radar` | Portal Radar | 1 | info | HUD line naming the next portal ahead and its distance in seconds ("SHIP in 1.2 s", "MIRROR", "DUAL"). | object index + `GameObjectType` (Ship/Ball/Ufo/Wave/Robot/Spider/Swing/Gravity/Size/Dual/Mirror/Teleport are all enum values). Speed portals are `Special` type → identify by `m_objectID` 200–203 / 1334 **(unverified)**. | Distance → seconds needs `m_playerSpeed`; approximate is fine. |
| `greed` | Greed | 3 | economy | Each death charges the gauge 25/50/75 % more. | `AugmentManager::onDeath` multiplier. | Snowballs; acceptable for a roguelite. |
| `persistence` | Persistence | 2 | economy | Gauge charges +1/+2 per second survived. | `postUpdate(dt)` → manager. | Practice/test attempts must not charge (use `isRunAttempt()`). |
| `bulwark` | Bulwark | 2 | forgiveness | A used shield comes back after 15/10 s without a hit. | shield fields + one timer. **Prerequisite:** Shield ≥ 1 (needs the `requires` system, §4). | — |
| `twinlink` | Twin Link | 1 | forgiveness | In dual mode, hits on player 2 are ignored. | `destroyPlayer` already discriminates `m_player2`. | Feel is unknown (P2 phasing through hazards while P1 plays); playtest before keeping. |
| `overdrive` | Overdrive | 1 | synergy | Shield break also triggers 3 s of 60 % speed. | noclip timer + `setGameSpeed`. Requires Shield ≥ 1 and Slow-Mo ≥ 1. | — |
| `haste` | Haste | 1 | curse | Game runs at 110 % speed; gauge charge +50 %. | time scale > 1 (same call as slow-mo; FMOD pitch > 1 untested but same API). | First curse — needs the `isCurse` flag (§4). |
| `glass` | Glass | 1 | curse | Shield is removed from the pool and disabled; draft threshold −30 %. | manager only. | Must be impossible to draft when it would leave the pool empty. |

### T2 — one new hook, verified binding

| id | name | lv | cat | effect | hook | risk / note |
|---|---|---|---|---|---|---|
| `steadycam` | Steady Cam | 1 | info | No camera shake. | `GJBaseGameLayer::shakeCamera(float, float, float)` — `win 0x23bc50`, not inline → return early. Fallback: zero `m_gameState.m_cameraShakeDuration` in `postUpdate`. | OpenHack does this with a byte patch ("No Shake"), so whether every shake routes through `shakeCamera` is **unverified**. |
| `quickrespawn` | Quick Respawn | 1 | QoL | Respawn 0.3 s after death instead of ~1 s. | GD schedules `PlayLayer::delayedResetLevel` (`win 0x3b8cf0`) from `destroyPlayer` with a constant delay (OpenHack sig-scans that float). After the original `destroyPlayer`, cancel and reschedule sooner. | Whether it is a scheduler selector or a `CCAction` is **unverified**. Low roguelite value — consider a plain setting instead of an augment. |

### T3 — unverified semantics, expect one approach change

| id | name | lv | cat | effect | approach | risk / note |
|---|---|---|---|---|---|---|
| `rewind` | Rewind | 3 | forgiveness | Auto-checkpoint every 2 s (ring of the last 3). On death, respawn at the newest checkpoint older than 1.5 s, with 0.5 s noclip. 1/2/3 rewinds per attempt. | `markCheckpoint()` on a timer inside the practice-mode wrapper used by Checkpoint; respawn path already written in `resetLevel`. | **Needs no hotkey**, so it is also the way to verify the checkpoint respawn path while M4 is blocked. Unknown: cost of `markCheckpoint()` in 10k-object levels; whether `m_checkpointArray` survives a normal-mode death. Read xdBot's checkpoint code before building. |
| `phoenix` | Phoenix | 1 | forgiveness | Once per attempt: die → respawn exactly where you died with 1 s noclip. | `markCheckpoint()` inside our `destroyPlayer` before the original call, then the Rewind respawn path. | Very strong; maybe fold into Rewind Lv3. Same unknowns as Rewind. |
| `reveal` | Reveal | 2 | info | Lv1: hitbox outlines only for invisible / faded solids and hazards (`m_isInvisibleBlock`, opacity < 30 %). Lv2: force those objects to ≥ 50 % opacity. | Lv1 is **T1** (Foresight subset, draw node). Lv2 hooks `GameObject::setOpacity` (`win 0x198800`) and must respect `m_opacityMod` / opacity groups. | OpenHack "Force Visibility" is a byte patch; the opacity pipeline is **unverified**. Ship Lv1 first. |
| `wideview` | Wide View | 2 | info | Camera zoomed out 10/20 %. | `m_gameState.m_cameraZoom` / `m_targetCameraZoom` exist; hook `GJBaseGameLayer::updateCamera(float)` (`win 0x23bcf0`) and apply a multiplier after the original. | GD may rewrite zoom every frame from triggers; BG parallax and UI may misalign. **Unverified.** |

### T4 — blocked by input (M4) or engine-deep

| id | name | lv | cat | effect | note |
|---|---|---|---|---|---|
| `ghost` | Ghost | 3 | forgiveness | Hold a key for up to 1.5/2.5/3.5 s of noclip per attempt. | T1 once keys work (`noclipTimer` infra). |
| `bullettime` | Bullet Time | 2 | time | Hold a key → 40 % speed, 3/6 s budget per attempt. | T1 once keys work. |
| `checkpoint` | Checkpoint (existing) | 3 | forgiveness | — | Blocked on the Z key. |
| `tiny` | Tiny | 1 | forgiveness | Player hitbox −20 %. | **No longer blocked (2026-09-17)** — it is now T2. `GameObject::getObjectRect(float, float)` (`win 0x1976c0`) is itself hookable and its two arguments are size factors, so scaling them scales what GD collides with; qolmod's HitboxMultiplier is the precedent and `wave-hitbox` (`PlayerHitboxHook.cpp`) is our build of it. `tiny` would be the same hook without the `m_isDart` gate. |
| `trajectory` | Trajectory | 1 | info | Predicted path line. | Needs a physics simulation. Rejected for now. |

### Rejected (keep the reasons)

| idea | why |
|---|---|
| Speed-portal changes | breaks music sync (DESIGN). |
| Input buffering / coyote time | too deep (DESIGN). |
| ~~Player hitbox shrink~~ | **Un-rejected 2026-09-17**: `getObjectRect(float, float)` is hookable and its arguments are size factors (qolmod HitboxMultiplier). Built as `wave-hitbox`. |
| Warp / later StartPos | starting past 0 % is not a clear; breaks the run model. (`setStartPosObject` is `win inline` anyway.) |
| Auto-jump, instant complete, auto coins | plays for you. |
| Disable fade / pulse / flash triggers | `GJEffectManager` too deep. |
| Force mini mode | changes physics, not just the hitbox. |

## 4. Systems

### T1 — no GD dependency

| system | what | why / note |
|---|---|---|
| **Pool metadata** | Add to `AugmentDef`: `category`, `rarity` (weights), `isCurse`, `requires` (ids with min level), `excludes`. `rollDraft` filters on `requires`/`excludes`, weights by rarity, allows ≤ 1 curse per draft. | Needed by Bulwark, Overdrive, Haste, Glass. Do this **before** adding those augments. |
| **Reroll** | N rerolls per run (setting, default 1). Button on the draft popup; pick stays mandatory. | Cheap; big feel improvement once the pool is > 10. |
| **Rising threshold** | Threshold grows by a settable % per draft taken. | DESIGN already lists it as a tuning idea; one line in `gaugeThreshold()`. |
| **Run summary** | Popup on `levelComplete`: deaths, drafts, augments, time (`PlayLayer::m_attemptTime` summed per attempt or our own clock), then `endRun()`. | Closes the loop emotionally; pure UI. |
| **Debug grants** | Setting or hidden button: start a run with a chosen augment set. | Cuts verification rounds for every later feature. |
| **HUD on-screen buttons** | Clickable Slow-Mo / Checkpoint buttons on the HUD (a `CCMenu` in `m_uiLayer`, like GD's pause button, which swallows the touch instead of jumping). | **No-key fallback for M4.** If keys stay dead, this unblocks Checkpoint, Ghost and Bullet Time. Touch-priority behaviour is a cocos fact, not GD, but still untested here. |

### T2 — Geode API only

| system | what | note |
|---|---|---|
| **Run persistence** | Serialize the manager (level id, augment levels, gauge, deaths, best %, slow-mo toggle, death marks) to `Mod::get()->setSavedValue` on every change; keyed by level id so several runs can coexist. Load on startup. | Geode saved-values API is documented; untested in this mod. Needed for "Continue" to survive a restart. |

### T3 — GD save path

| system | what | note |
|---|---|---|
| **Safe mode** | Augmented clears must not count as GD clears before any public release. Candidates: hook `GJGameLevel::savePercentage(int, bool, int, int, bool)` (`win 0x16c8b0`) and `GameStatsManager::completedLevel` (`win 0x1de3b0`) and skip when the finishing attempt was a run attempt. Alternative: set `m_isTestMode` for the duration of `levelComplete` (side effect: test-mode complete screen). | **Unverified.** Decide only after reading how an existing safe-mode Geode mod does it. |

### T4

| system | what | note |
|---|---|---|
| **Hotkeys** | The open problem in `GD-INTERNALS.md`; also the acceptance test of `HARNESS-PLAN.md` (Phase 4). | Read how QOLMod / click-between-frames receive keys on Geode 5 first, then the listed steps (≤ 2 rounds); if still dead, ship the HUD-button fallback and depend on `geode.custom-keybinds` later. |

## 5. Recommended order

1. **M4 attempt, time-boxed to 2 rounds** (log-every-key, then `UILayer::keyDown`). In the same build, add the **HUD on-screen buttons** so Checkpoint becomes testable either way.
2. **Pool metadata + reroll** (T1 systems). Small refactor; every later augment needs it.
3. **T1 augment batch** in this order — each is one build/test round: Greed, Persistence (economy first, they make testing faster), Reflex (needs the shared object index refactor), Death Marks Lv1, Portal Radar, Bulwark, Overdrive, Haste, Glass, Twin Link. Pool reaches 15.
4. **Rewind** — verifies the checkpoint respawn path without keys; if it works, Checkpoint and Phoenix come almost free.
5. **Run summary + run persistence.**
6. **Steady Cam → Reveal Lv1 → Wide View** (T2/T3 information augments), one at a time.
7. **Safe mode**, then decide on a release.

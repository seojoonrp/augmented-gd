# Design notes

A level becomes a **run**: you keep dying, every death feeds a gauge, a full
gauge gets you a draft, and augments stack up until the level is cleared.
Dying keeps your augments; clearing ends the run.

## Run

- Started with the round `AUG` button on the level info screen. There is
  **one run at a time** across all levels, kept in memory only (closing GD
  ends it).
- `AUG` on the level that has the run offers Continue / Restart. On any other
  level it asks first, because starting there drops the parked run (Cancel /
  Start). Both prompts are `RunPromptPopup`.
- **Only the AUG button enters a run.** It arms the next `PlayLayer` of that
  level (`AugmentManager::armRunEntry`). GD's own Play button disarms, and any
  way in that wasn't armed is a normal attempt (no augments, GD records as
  usual) with the run left parked.
- A run ends on `levelComplete`. Leaving the level does not end it.
- Practice / test-mode attempts don't count and get no augments.

## Records

Run attempts never touch GD's records: no normal-mode percent is saved, GD's
New Best! doesn't fire, and a run clear isn't a GD clear (`levelComplete` runs
with `m_isTestMode` borrowed, the end screen says "Cleared with augments!").
GD's attempt counter still counts. Records GD already had from before stay.

Three bests, kept apart:

| best | scope | used for |
|---|---|---|
| GD's normal best | GD | untouched by runs |
| run best (`RunState::bestPercent`) | one run | the gauge's new-best bonus, the gold dot on the progress bar |
| run record (`src/game/Records.hpp`) | per level, all runs, saved | GD's New Best! popup when beaten |

The record is a whole percent compared like GD does (4.1 -> 4.4 is not a new
best). It's checked on every death, checkpoint deaths included, since it's
about how far a run got. A run clear sets it to 100 (no popup, the end screen
shows instead). The popup is GD's own `showNewBest` with no orbs or diamonds.

## Draft gauge

Every run opens with a **free draft**, shown once the level is on screen
(`PlayLayer::startGame`). After that each death charges the gauge:

```
charge    = floor(percent)
          + (floor(percent) - floor(best)) * NewBestBonusMult   (only if > 0, mult 1.0)
threshold = min(GaugeThresholdStart + GaugeThresholdStep * (gaugeDrafts / GaugeThresholdEvery),
                GaugeThresholdMax)                               (20 x3, 25 x3, ... 65 x3, 70 ...)
```

- Charges are whole percents, so the bar always reads exactly what it holds
  (with decimals a gauge of 29.6 would read `30/30` and not draft).
- Reaching the threshold queues a draft for the next reset from 0 and raises
  the threshold. A big new best can queue several; the popups then chain.
  Leftover charge carries over. The opening draft doesn't raise the cost.
- Never more drafts than levels left to give (`RunState::levelsLeft`), so the
  last level of the run comes as a single card. Once everything is maxed the
  bar stays full and reads `MAX`.
- While a draft waits, the bar is full and shows that draft's cost
  (`RunState::lastDraftCost`), e.g. `30/30`.
- New bests count in whole percents; the best itself keeps its decimals for
  the HUD.

**A life pays once.** A life is one go from 0 %. A death that a checkpoint
brings you back from charges nothing; the death that really ends the life
pays for the furthest point the life reached (`RunState::onDeath`, with
`respawning` answered by `StartPos::onDeath`). Dying at 6 %, respawning, and
dying at 4 % is worth 6, not 10. The deferred death shows no reward at all;
everything plays on the settling death.

Known issue: a life left hanging by a quit or manual restart isn't dropped,
but it isn't paid in full either. Its `lifeBest` carries into the next life
and only the larger of the two is paid. Fix would be to settle it on the next
reset from 0.

Why no minimum charge: a flat floor (`max(10, percent)`) made dying at 0 %
over and over the fastest way to a draft. Without it a 3 % death is worth 3,
so farming doesn't pay, while being stuck at 60 % still pays 60 per death. The
new-best bonus pays new ground twice and is capped at `100 * mult` per run, so
it can't be farmed either. The rising threshold keeps late-run drafts from
flooding in.

**Debug mode** (`debug-mode` setting): number keys 1-9 grant augments in table
order, Shift+1-4 the 10th onwards (cat, brake, missile, berserker), 0 fills
the gauge to the threshold (the draft still comes on the next death). The
top-left readout (run numbers, each augment's state) needs `debug-readout`
on top of it, so debug mode can be used for screenshots.

## Draft

Three random augments that aren't maxed yet, shown on respawn. Picking is
mandatory (no close button, back key ignored). Picking one you have levels it
up. `draft-count` makes it 4 cards from the next draft on; the popup narrows
the cards so four still fit.

## Augments

Names and card texts are in `src/core/AugmentDef.cpp` (English and Korean),
with the numbers pulled from `tune::` so the card and the game can't disagree.
Behaviour is one file per augment in `src/augments/`.

| id | name | max | what it does | notes |
|---|---|---|---|---|
| `shield` | Shield / 결계인가? | 5 | one shield per level every attempt; a broken shield gives 1.5 s of noclip | covers both players in dual; a checkpoint snapshots the charges left |
| `slow-mo` | Sloth / 나무늘보 | 3 | game speed -5 % per level, X toggles | game + music; pause menu runs at normal speed |
| `startpos` | Checkpoint / 스타트포스 | 3 | Z places a checkpoint, one placement per level per attempt | only the newest placement is live; a death respawns there once, the next death restarts from 0. restores shield charges and brake seconds from when it was placed |
| `foresight` | Foresight / 사륜안 | 1 | shows hitboxes | GD colours: blue solid, red hazard, green interactive, yellow player |
| `unmirror` | Unmirror / 멀미약 | 1 | mirror portals do nothing | `toggleFlipped` hook; drafting it un-flips right away |
| `hazard-hitbox` | Threat Removal / 위협제거 | 7 | hazard hitboxes -5 % per level | shrinks around the centre; solids, slopes and the player untouched |
| `wave-hitbox` | Wave Breaker / 웨이브브레이커 | 7 | player hitbox -10 % per level in wave | per player, so in dual only the half in wave shrinks; the icon and trail shrink too. rotated hazards use the player OBB, which this doesn't touch |
| `nerve` | Calm Nerves / 청심환 | 1 | both shrinks grow with progress: (120 + X/2) % of themselves at X % | either scale is floored at `tune::MinHitboxScale` (0.2) |
| `draft-count` | Opportunity Cost / 기회비용 | 1 | 4 cards per draft from the next draft | |
| `cat` | Cat / 고양이 | 7 | every 4 s removes 5 random hazards in view | Lv2-5: +2 hazards, -0.5 s. Lv6-7: +4 hazards (`tune::CatLateLevel`). only hazards ahead of the player; solids and slopes are never removed (that would break routes). a doodle cat sits in the bottom-right corner and casts a magic circle on each removed hazard |
| `brake` | Brake / 브레이크 | 3 | hold C for 40 % speed, 7 s per level per attempt | absolute, ignores slow-mo; real seconds; a level-up mid-attempt adds its 7 s at once |
| `missile` | Air Raid / 공습경보 | 5 | every 6 s a missile hits a random hazard in view and clears every hazard within 3 blocks | Lv2+: +1 block, -0.5 s. aims at a hazard at least 300 units ahead so it lands before you get there; with nothing in view it waits and fires as soon as one shows up |
| `berserker` | Berserker / 버서커 | 3 | every destroyed hazard has a 3 % chance to open a 2.5 s window where hazards you touch get destroyed | +1.5 % per level. only the cat, the missile and berserk itself destroy hazards, so it needs one of them (the HUD row says so). a smash rolls again. asked before the shield so a free smash never spends a charge. solids, slopes and deaths with no object still kill |

Every removal (cat, missile, berserk) goes through `hazard::Removed`
(`HazardRemoval.hpp`): it sets GD's `destroyObject()` flags and puts
everything back on reset. Timers run on game time, so slow-mo stretches them.

## Text and fonts

Player-facing text comes in English or Korean, picked by the `language`
setting (English by default). GD's fonts have no Hangul, so the mod ships its
own: ImcreSoojin for the UI, baked GD-style (white, black outline, shadow) by
`scripts/fontgen.py`, and Pretendard for the debug HUD. Both charsets come from
the string literals in `src/` (`scripts/fontcharset.ps1`). Logs and debug
readouts stay English. The AUG prompts' buttons are GD `ButtonSprite`s in
goldFont, so they're English in both languages.

## UI

**Draft cards** look like GD buttons: white rim, green body with a black
ring, darker footer. Top to bottom: name, art, description, footer with
`NEW` / `Lv a -> b` and level stars. Cards fan out from the centre and can't
be picked until they land. Card art is `resources/augments/<id>.png`
(480x280, Geode bakes hd/sd); a missing file just leaves the panel empty, so a
new augment needs an image, not code.

**AUG button**: the mod's mark in a round green `CircleButtonSprite`, left of
the difficulty face at the Play button's height. The mark is cropped to its
own bounds by `scripts/logocrop.py` because the button fits whatever it gets
to 65 % of the circle.

**HUD**: the draft gauge is its own rounded bar at the bottom centre with
`DRAFT` and `charge/cost` over it (`draft-bar-opacity` setting fades it). A
death plays a short reward: particles fly from the icon into the bar, gold
ones and a `+X` for a new best; the whole thing ends before GD respawns. GD's
progress bar gets two dots: gold at the run's best, green at the live
checkpoint. Short notices (checkpoint placed, respawned, shield broke) rise
in near the bottom-left corner; `BERSERK!` is a bigger red banner above the
gauge. The augment list and stats on the left are debug-mode only; in normal
play the pause menu has them.

**Pause menu**: on a run level the practice button is replaced by the mod's
round button (practice attempts aren't run attempts anyway). It opens the run
summary (`RunInfoPopup`) in GD's menu colours: deaths, session best and total
best, then a tile per held augment (art, name, level). Hover grows a tile, a
click opens the augment's card with the text for the level held
(`AugmentDef::describeAt`). The grid picks the column count that gives the
biggest tiles, so it never scrolls.

## Not decided yet

- Gauge numbers are tuned by playing; they'll keep moving.
- Runs don't survive a game restart (in memory only).

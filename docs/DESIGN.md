# Augment Mode — game design

A level becomes a **run**: attempts accumulate augments until the level is
cleared. Dying keeps your augments; clearing ends the run.

## Run

- Started from the level info screen (`AUG` button → Start). One run per level
  ID at a time; starting again on the same level offers Continue / Restart.
- Ends on `levelComplete`. Leaving the level does **not** end it.
- **Only the AUG button enters a run** (2026-09-29, user: a run left mid-way
  came back when the level was played normally). The AUG button (new run,
  Continue or Restart) arms the next `PlayLayer` of that level
  (`AugmentManager::armRunEntry`); GD's own Play button disarms, and any
  way in that was not armed plays a normal attempt — no augments, GD's
  records as usual — with the run parked until the next Continue.
- Practice / test-mode attempts never count and get no augments.
- **Run attempts never touch GD's own records** (2026-09-28, user: a run on a
  level with no record wrote its percent into GD's). No normal percent is
  saved, GD's New Best! does not fire, and a clear with augments is not a GD
  clear — `levelComplete` runs with `m_isTestMode` borrowed, and the end
  screen's quote reads "Cleared with augments!" (English is fine there; the
  user dropped a second "Normal progress is not saved." sentence). GD's attempt counter still counts.
  Records GD already holds from earlier runs cannot be told apart and stay.

## Records (2026-09-28)

Three bests, kept apart:

| best | scope | what it drives |
|---|---|---|
| GD's normal best | GD's own | untouched by runs |
| run best (`RunState::bestPercent`) | one run | the gauge's new-best bonus (gold `+X`), the gold dot |
| **run record** (`src/game/Records.hpp`) | per level, all runs, Geode saved values (survives restarts) | GD's **New Best!** popup when beaten |

The record is a whole percent, compared like GD (4.1 → 4.4 is not one). It is
checked on **every** death, a checkpoint respawn's included — it is how far a
run got, not what the gauge has settled — so on such a death New Best! can
show while the gold `+X` waits for the end of the life. A clear with augments
sets it to 100 (no popup; the end screen shows instead). The popup is GD's own
`showNewBest` with no rewards (no orbs, no diamonds). The HUD header shows it
as `record N%`.
- Public release note (2026-09-16): `$GEODE_SDK/AGENTS.md` states the Geode
  index does not accept AI-written mods. Private use is unaffected; any index
  submission is the user's call.

## Draft gauge (redesigned 2026-09-17)

Every run opens with a **free draft**: `startRun` queues it and the
`PlayLayer::startGame` hook shows it once the level is on screen (the user
rejected showing it on the level-info screen, 2026-09-17). After that, each
death charges the gauge:

```
charge = percent                                             (no floor)
       + (floor(percent) - floor(best)) * NewBestBonusMult   (only when > 0, mult 1.0)
threshold = min(GaugeThresholdStart + GaugeThresholdStep * gaugeDrafts,
                GaugeThresholdMax)                           (30, +5 each, stops at 100)
```

Each time the gauge reaches the threshold a draft is queued for the next
from-0 reset and the threshold rises; a big new best can queue several at
once, and the popups then chain (pick → next popup, game stays paused).
Leftover charge carries over. Only gauge-earned drafts raise the threshold
(the opening draft does not). Numbers
live in `tune::` (`AugmentDef.hpp`) and are tuned by test. A new best counts
in whole percents (4.1 → 4.4 is none; the user found `NEW BEST +0` annoying,
2026-09-20) while the best itself keeps its decimals for the HUD and the dot.
When the bonus fires the HUD shows `NEW BEST` and a gold `+X` beside the dead
icon (`RunHud::playDeathReward`); while a gauge-earned draft waits the gauge
readout says `DRAFT!` instead of numbers (the cost has already risen, so
`50/50` would show the *next* cost).

**A life pays once** (2026-09-27): a death a checkpoint brings the player back
from charges nothing. A life — one visit to the level from 0 % — is settled by
the death that really ends it, for the furthest point the whole life reached
(`respawning` in `RunState::onDeath`, answered by `StartPos::onDeath`, which
already knows whether a respawn follows). Dying at 6 % and then, after the
respawn, at 4 % is worth 6, not 10 (the user's report). The deferred death
still counts as a death, but nothing is settled, so it shows no `NEW BEST`, no
`+X` and no particles — the whole reward plays on the settling death, at the
life's best. The best percent is held back with it (otherwise the new-best
bonus would be paid before the life is), so the HUD folds `RunState::lifeBest`
into the best it draws and the white mark never slides back to the checkpoint.
A life left hanging by a quit or a manual restart is not lost: its best is
charged by the next death that settles, which comes to the same total.

Rationale (2026-09-17): the old flat floor (`max(10, percent)`) made
"die at 0 % ten times" the fastest route to a draft. Without the floor a 3 %
death is worth 3, so farming never pays, while being stuck at 60 % still pays
60 per death. The new-best bonus rewards GD's core achievement (new ground is
paid twice), and its total over a run is bounded by `100 * mult`, so it cannot
be farmed either. The rising threshold stops late-run draft floods (stuck at
80 % used to mean a draft every two deaths). Rejected: a decaying floor
(the floor was dropped instead).

**Debug mode** (`debug-mode` setting, default off): number keys 1–9 grant
augments (Shift+1–9 the 10th onwards, i.e. Shift+1 = cat, Shift+2 = brake, Shift+3 = missile,
Shift+4 = berserker) and key 0 tops the gauge up to the threshold (the draft
still happens on the next death). The cost is the normal ramp: the fixed
`debug-threshold` setting is gone (2026-09-27), since it meant testing an
economy nobody plays.

## Draft

Three random augments that are not yet maxed, shown on respawn. Picking is
mandatory (no close button, back key ignored). Duplicate picks level the
augment up. Pool is 13 augments, all implemented. Owning `draft-count` raises
the card count to 4 from the next draft on; the popup then narrows the cards
and scales their text by the same ratio so four still fit GD's 569 pt width.

## Augments (design table, applied 2026-09-17)

Names and descriptions are Korean and shown verbatim on the cards: the
*initial* text when drafting level 1, the *level-up* text for every later
level. The id is the "코드" column and is what the hooks key on.

| id | name | max | initial | level-up | status |
|---|---|---|---|---|---|
| `shield` | 결계인가? | 3 | 매 어템마다 보호막이 지급됩니다. 보호막이 깨지면 1.5초간 노클립 상태로 전환됩니다. | 보호막 개수가 하나 늘어납니다. | working. Covers both players in dual. Charges left at checkpoint placement come back on the respawn (verified 2026-09-17). |
| `slow-mo` | 나무늘보 | 3 | 게임 속도가 5% 감소합니다. X를 눌러 토글할 수 있습니다. | 게임 속도가 5% 더 감소합니다. | working. Speed = 1 − 0.05·level (95/90/85 %), game + music; toggle state persists within the run; pause menu at normal speed. |
| `startpos` | 스타트포스 | 5 | 매 어템마다 Z를 눌러 체크포인트를 찍을 수 있습니다. 해당 어템에 죽으면 체크포인트에서 부활합니다. | 체크포인트를 한 번 더 찍을 수 있습니다. | working (v3 verified 2026-09-17). `level` placements per attempt (a life from 0 %), but **only the newest one is live**: placing again moves it; a death respawns there **once**, and the next death restarts from 0 unless a new one was placed in between (budget permitting). The older "each checkpoint is one respawn, newest first" chain was dropped as too loose (user, 2026-09-17). A checkpoint also **snapshots shield charges and brake seconds left** and the respawn restores them (`Augment::onCheckpointPlaced` / `onCheckpointRespawn`). A death the respawn comes back from **charges no gauge**: the life pays once, at its best percent (2026-09-27, see the gauge section). |
| `foresight` | 사륜안 | 1 | 히트박스를 보여줍니다. | – | working. GD colours: blue solid, red hazard, green interactive, yellow player. |
| `unmirror` | 멀미약 | 1 | 레벨 내 모든 미러포탈을 제거합니다. | – | rewritten 2026-09-17 as a `GJBaseGameLayer::toggleFlipped` hook (refs pattern), untested: flips are refused while owned, drafting un-flips at once. |
| `hazard-hitbox` | 위협제거 | 5 | 위험 요소(빨간 히트박스)의 크기가 5% 감소합니다. | 위험 요소의 크기가 5% 더 감소합니다. | built, in-game test pending. Hazard / AnimatedHazard hitboxes shrink around their centre to 1 − 0.05·level (95…75 %); solids, slopes, player untouched. |
| `wave-hitbox` | 웨이브브레이커 | 5 | 웨이브 모드일 때 플레이어 히트박스 크기가 10% 감소합니다. | 웨이브 모드일 때 플레이어 히트박스 크기가 10% 더 감소합니다. | working (verified 2026-09-17). Player rect × (1 − 0.10·level) while `m_isDart`; checked per `PlayerObject`, so in dual only the half that is in wave shrinks. The wave icon and trail thickness shrink by the same factor (node scale `m_vehicleSize × scale`, 2026-09-20, unverified in game). Rotated hazards use the player OBB, which this does not touch (`GD-INTERNALS.md`). |
| `nerve` | 청심환 | 1 | 레벨 후반에 도달할수록 [위협제거]와 [웨이브브레이커]의 효과가 증가합니다. X% 도달 시 두 능력의 효과가 각각 X% 증가합니다. | — | working (verified 2026-09-17). Both *shrinks* × (1 + progress), progress = current percent / 100. Was 2 levels (k = 1.5 at Lv2); fixed to a single level 2026-09-20. Either scale is floored at `tune::MinHitboxScale` (0.2), which wave-hitbox Lv5 + nerve at 100 % would otherwise hit exactly (shrink 1.0). |
| `draft-count` | 기회비용 | 1 | 다음 드래프트부터 카드가 4개씩 등장합니다. | – | working (verified 2026-09-17). `rollDraft(4)` from the next draft on; the popup narrows the cards to fit. |
| `cat` | 고양이 | 5 | 마법 고양이를 소환합니다. 고양이는 4초마다 시야에 있는 장애물 5개를 랜덤으로 제거합니다. | 고양이가 매번 장애물을 한 개 더 제거하고, 제거 쿨타임이 0.5초 감소합니다. | built 2026-09-17, in-game test pending. Every `4 − 0.5·(lv−1)` s of play, `5 + (lv−1)` random **hazards** (Hazard / AnimatedHazard — solids and slopes are never "장애물" here, removing them would break routes) that are on screen *and ahead of the player* are removed for the rest of the attempt (sprite + hitbox, via GD's `destroyObject()` flags). Restored on every reset. A placeholder square sits bottom-right and **casts a small magic circle** on each removed hazard (two rings, 4 rim ticks, one spinning triangle, r16 screen units, 0.55 s, **white** — plainer and white after the first look, user 2026-09-27) — it replaced the lasers the square used to fire (user 2026-09-27: "그냥 마법으로"). Real cat art later. |
| `brake` | 브레이크 | 3 | C를 누르고 있으면 게임 속도가 60% 감소합니다. 어템마다 최대 7초씩 사용할 수 있습니다. | [브레이크]를 어템마다 7초 더 사용할 수 있습니다. | working (verified 2026-09-17). Held key (C, `keybind-brake`): game + music at **40 %** while held, regardless of slow-mo (the cut is absolute, decided with the user 2026-09-17); budget `7·level` **real** seconds per attempt (a from-0 reset refills; a checkpoint respawn restores the seconds left when the checkpoint was placed), a mid-attempt level-up adds its 7 s at once. Running out is log-only (its notice went with the centre texts, 2026-09-27). Pause forgets the held key (focus loss sends no release). Implemented as a time *override* layer in `Scales.cpp` that wins over the slow-mo base speed. |
| `missile` | 공습경보 | 5 | 6초마다 시야 내 위험 요소 하나에 미사일이 떨어집니다. 반경 3칸 안의 위험 요소가 모두 제거됩니다. | 폭발 반경이 0.5칸 커지고, 미사일 쿨타임이 0.5초 감소합니다. | working (verified 2026-09-20, lead 300 "딱 괜찮은듯"). Every `6 − 0.5·(lv−1)` s of play a missile is aimed at a **random hazard** on screen and at least 300 units (~1 s at 1x) ahead of the player (was 150; the user found the impact landed where they already were, 2026-09-20 — aiming at a hazard, not a point, so every strike hits something; nothing in view → the strike stays armed and fires as soon as a hazard scrolls in). It drops for 0.35 s (world-space reticle + missile, `MissileNode` in `m_objectLayer`, **all white** since 2026-09-27 — the depth the orange carried is alpha now; the `MISSILE -n` notice is gone), then every hazard whose collision shape (AABB, or radius for saws) touches the blast circle of `3 + 0.5·(lv−1)` blocks is removed for the rest of the attempt — same `destroyObject()` flags and put-back as the cat (`HazardRemoval.hpp`). A reset mid-drop cancels the missile. |
| `berserker` | 버서커 | 3 | 위험 요소가 파괴될 때마다 3% 확률로 2초간 버서커 모드에 돌입합니다. 버서커 모드에서는 부딪히는 위험 요소가 모두 파괴됩니다. | 버서커 모드 발동 확률이 1% 증가합니다. | built 2026-09-27, in-game test pending. Every hazard **an augment destroys** rolls `0.03 + 0.01·(level−1)` (3 / 4 / 5 %) to open a 2 s window; while it is open, `destroyPlayer` with a **hazard** object smashes that hazard (same `destroyObject()` flags and put-back as the cat and the missile) instead of killing the player. A smash is itself a destroyed hazard, so it rolls again and can refresh the window. Solids, slopes and a death GD names no object for (`object == nullptr`, e.g. suicide) still kill. The window is flat across levels (only the chance grows) and is **game** seconds (slow-mo stretches it, like the cat and missile timers); it closes on any reset. Retuned with the user 2026-09-27: 2 % / 3 s at Lv1 became 3 % / 2 s, and the red screen frame landed at 5 bands x 7.5 units, 0.33 alpha, pulse 0.72 +- 0.28 (the middle of three passes: 6 x 9 / 0.5 was "너무 과함", 4 x 6 / 0.2 too little). A `BerserkAura` (`src/ui/BerserkAura.hpp`) also puts a flickering fire crown on the player while the window is open, in the object layer at `player z - 1` so the icon draws on top. Asked before the shield (`Augment::hitPriority`) so a free smash never spends a charge. **Dependency:** nothing else destroys hazards, so without `cat` or `missile` it can never roll — the HUD row says `needs cat/missile`. |

Definitions and tuning constants live in `src/core/AugmentDef.*` (the
description text quotes the numbers as literals, so change both together);
behaviour in `src/augments/` (one file per augment, see `Augment.hpp`). Debug keys 1-9 grant augments in
table order, Shift+1-9 continue from the 10th (Shift+1 cat, Shift+2 brake, Shift+3 missile,
Shift+4 berserker).

## Text & fonts (decided 2026-09-17)

In-game augment text and the four bottom-left notices are Korean; logs and
the HUD's non-name words stay English. GD's fonts have no Hangul, so the mod ships its own
(`resources/fonts/`). Player-facing UI (draft cards, popup title,
notices) uses 아임크리수진 (`ImcreSoojin.ttf`) rendered GD-style — white
glyphs, black outline, drop shadow — baked by `scripts/fontgen.py`. Debug
readouts (the HUD lines) stay in plain Pretendard Regular, generated by Geode.
Both get their charset from the sources (`scripts/fontcharset.ps1`); see
`docs/GD-INTERNALS.md` "Fonts".

## Draft card look (decided 2026-09-17)

GD button styling: white rim, green body with black ring, darker footer band;
top to bottom — name, image box, description,
footer with `NEW` / `Lv a → b` on the left and level pips (gold ★ reached,
grey ☆ remaining) on the right. Cards fan out from the centre when the draft
opens and can't be picked until they land.

**Card art** (2026-09-27): one drawing per augment in `resources/augments/`,
named after the augment id (`slow-mo.png`), 480x280 px — the image slot
(120x70 pt) at uhd, which is what Geode's `resources.sprites` wants: it bakes
the hd and sd copies itself. `AugmentDraftPopup::createCard` builds the name
from `def.id` at runtime (`Mod::expandSpriteName`), scales it to fit the white
panel inside the black border (116x66 pt, aspect kept) and leaves the panel
empty with a warning if the file is missing — so a new augment needs a file,
not a code change.

## Mark and the AUG button (2026-09-27)

The mark is the user's own drawing. First version (2026-09-27, after a
generated one they turned down): a level card tilted behind a white arrow
pointing up, near-white fills with `#2D2D2D` strokes. **Second version**
(2026-09-29): a grey GD cube tilted behind a red arrow pointing up. Two
exports: `logo.png` at the root (336) is what the Geode mod list shows, and
`resources/ui/aug-logo.png` (**256** since the second version, transparent)
is the glyph in the game. The glyph is the drawing cropped to its own bounds
and padded 2 % into a square (`scripts/logocrop.py`), because the button
fits whatever it is given to 65 % of the circle and any margin inside the
file only makes the mark smaller (a raw 672 export, drawn in 72 % x 59 % of
its canvas, came out small). 256 px, not the export's size: the buttons
draw the glyph at ~125 (level page) to ~147 (pause menu) uhd pixels, GD's
textures have no mipmaps, and shrinking more than 2x makes thin strokes
jagged — a 336 export looked broken to the user. The vector lives with the
user (`Desktop/augmented-gd/logo`), not in the repo; run each new export
through `logocrop.py`. Geode does **not** round
a logo's corners — `createModLogo` just scales the file — so the rounding
in `logo.png` is the user's own (2026-09-27).

On the level info screen that glyph sits in a round green button
(`CircleButtonSprite`; the glyph fills the 65 % it is fitted to,
`setTopRelativeScale` left at 1.0 once the file's own padding came down to
2 %). **Since 2026-09-29** it is **halfway in size between its old Medium
circle and GD's Play button** — `(46.75 + play width) / 2`, the Play
button's width read at runtime from `m_playBtnMenu` — drawn from the Large
circle (321 uhd px) scaled down, and it stands **left of the difficulty
face, level with the Play button**: `LevelInfoHook::augmentButtonSpot`
takes the Play button's height and `m_difficultySprite`'s left edge minus
12 pt minus the radius. Fallbacks (no Play button: 66 % of the screen
height; no difficulty sprite: x = 122) are logged. Before that it sat
beside GD's copy button in the left column (three rounds: centred at x 78,
then 98, then the copy-button row).

## HUD (v2, 2026-09-17)

Kept small so it stays out of the way. The **draft gauge** is a twin of
GD's progress bar mirrored to the bottom edge (same sprite and scale, fill
in the draft cards' green, eased) with `charge/cost` (e.g. `23/30`) in GD's
percent font at its right end; while a gauge-earned draft waits it reads
`DRAFT!` — the free opening draft does not fill it. At the far left, a grey
header (deaths, live %, best %) and then one **row per owned augment**, top
to bottom: a framed placeholder box where the icon will go, the Korean name,
and its English per-attempt state. GD's own progress bar gets rimmed dots:
gold at the run's best (moves with the player past it, and the colour of the
new-best particles), green at the live checkpoint (`ProgressMarks`). Both are
`0.375` of the fill track's height, so they sit inside it instead of poking
out of the bar (user screenshot, 2026-09-27). Decided with the user over four rounds on
2026-09-17. The two hitbox lines
show the scale *at the player's current position*, so they move as nerve ramps
up; 웨이브브레이커 adds `ACTIVE` while a player is in wave.

**Debug-mode only since 2026-09-28**: the grey header and the augment rows
are drawn only with the `debug-mode` setting on (the user wants them kept for
debugging); in normal play the pause menu carries that information. The
gauge, the progress-bar dots and the notices always show.

## Pause menu (2026-09-28)

On a run level the pause menu's practice button is replaced by the mod's
round button (the AUG button's face, sized to the practice button) — a
practice attempt is not a run attempt, so a run has no use for it; it stays,
with ours beside it, only if the player is already in practice mode. The
button opens the **run summary** (`RunInfoPopup`) in **GD's own menu
colours** (round 7, after a CreatorLayer screenshot from the user, values
sampled from it): the card (`card::framedPanel`) keeps the white rim, black
ring and soft shadow around GD's menu blue as a vertical gradient
(0/96/241 → 0/56/142) — the thin white inner stroke went in round 8, and
the AUG resume prompt now sits on the same card; the stat chips and the tiles are
**GD menu panels** (`card::gdPanel`) — white rim, black ring, a green body
shaded left to right in two halves (upper 200/254/89 → 107/208/19, lower
150/252/62 → 70/162/13, 49 % / rest) over a dark strip along the bottom
(75/127/30 → 38/84/9), and a black 40 % shadow down-right that reads dark
blue on the card. (Rounds 2–6 had a green gradient card with dark
translucent chips and tiles; the user found the colours off.) Three stat chips on top, captions in English
per the user — **Deaths** (this run), **Session Best** (this run's best,
whole percent, not counting the attempt in progress), **Total Best** (the
runs' record, caption and number both gold) — then `보유 증강 (N)` in white with an engraved
rule to the right edge, and one tile per held augment in table order: name,
the card art in the draft card's 120x70 slot, `Lv N` (gold when maxed) and
level stars. The three chips are a centred row of 112x30 panels, 12 pt
apart, with smaller text (caption 0.38, number 0.48 — round 7). No
descriptions (user). Paddings are 14 pt from the ring, 8 pt between tiles,
chips → section rule → grid 8 / 6 pt (round 5), and chip text is centred by
its real label heights. A tile keeps 14 pt free at
each side (its art slot is 104x61, the draft card's shape made smaller), so
tiles read smaller with more side room, and four in a row fill the card's
width exactly (round 5, user).

**Tile detail** (2026-09-28, round 4): under the mouse a tile eases up to
106 % — size only, the white-border highlight of rounds 4–6 went in round 7
(polled each frame — cocos has no hover event; the hit area stays put) —
and a click opens `AugmentInfoPopup`: the draft card itself
(`card::augmentCard`, shared with the draft popup) centred at 1.1x (1.3 in
round 4 filled the screen's height), with the text of what the augment
does **at the level held** — `AugmentDef::describeAt(level)`, the initial
sentences with that level's numbers from `formula::` (shield Lv3 "보호막이
3개", cat Lv3 "3초마다 … 장애물 7개", missile Lv3 "5초마다 … 반경 4칸"),
host-tested so the card and the game cannot disagree — `Lv N` in the
footer (gold once maxed) and N stars. Close / Esc returns to the summary.

**Level stars** (2026-09-28): the user's rounded star
(`resources/ui/round-star.svg`, baked to `round-star.png` by
`scripts/stargen.py` — white fill inside a black outline) tinted gold for
levels held and grey for the rest, 3.8 pt radius, 7 pt apart (round 7),
tipped 12° to the left (round 6), on the draft cards and the tiles alike
(`card::stars`). Two drawn versions came first:
ImcreSoojin's ★ glyph carries a stray mark above the star, and a
CCDrawNode rounded star threw long spikes (round 4 screenshot). The tiles are drawn at full size and scaled as a whole; the grid
takes the column count that gives the biggest tiles (full size at most, the
fuller grid on a tie — 4 augments make 2x2) and the card grows with it up to
292 pt, so it never scrolls: 3 augments sit at full size, 13 at ~55 %.
Laid out after the user's references (Vampire Survivors-style upgrade grids).

**Notices** (2026-09-27): the screen used to carry a big centred line for every
event, which the user read as debug text and asked to have gone. What is left
are four short Korean lines in the **bottom-left** corner (UI font, white with
the baked outline, scale 0.45), each shown at once, held ~1.1 s and faded over
0.45 s; a second line while one is still up pushes it a line higher, three at
most (`RunHud::notice`).

| when | line |
|---|---|
| checkpoint placed or moved | 체크포인트가 설정되었습니다. |
| respawned at a checkpoint | 체크포인트에서 부활합니다. |
| shield absorbed a hit | 보호막이 깨졌습니다. |
| berserk window opened (not a refresh) | 버서커! |

Everything else that used to pop up (`CAT -n`, `MISSILE -n`, `SLOW-MO ON/OFF`,
`BRAKE EMPTY`, `NEW BEST`, `NO CHECKPOINTS LEFT`, `CAN'T PLACE HERE`, the debug
grants) is log-only now; a new best is still visible as the gold `+X` beside
the dead icon.

## Not decided yet

- Draft gauge numbers (40 / +10 / bonus ×1.0) are first guesses; tune by test.
- Remaining augments and systems: candidates, difficulty tiers, rejected ideas
  and build order are in `ROADMAP.md` (2026-09-16).
- Run persistence across game restarts (currently in-memory only).

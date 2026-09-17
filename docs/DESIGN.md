# Augment Mode — game design

A level becomes a **run**: attempts accumulate augments until the level is
cleared. Dying keeps your augments; clearing ends the run.

## Run

- Started from the level info screen (`AUG` button → Start). One run per level
  ID at a time; starting again on the same level offers Continue / Restart.
- Ends on `levelComplete`. Leaving the level does **not** end it.
- Practice / test-mode attempts never count and get no augments.
- Level clears made with augments currently **count as normal GD clears**.
  Decided 2026-09-16: ignore for now; revisit before any public release.
  Mechanism when we do: flip `m_isTestMode` / `m_isPracticeMode` around
  `PlayLayer::levelComplete()` (see `docs/GD-INTERNALS.md` "Does a clear count?").
- Public release note (2026-09-16): `$GEODE_SDK/AGENTS.md` states the Geode
  index does not accept AI-written mods. Private use is unaffected; any index
  submission is the user's call.

## Draft gauge (redesigned 2026-09-17)

Every run opens with a **free draft**: `startRun` queues it and the
`PlayLayer::startGame` hook shows it once the level is on screen (the user
rejected showing it on the level-info screen, 2026-09-17). After that, each
death charges the gauge:

```
charge = percent                                   (no floor)
       + (percent - best) * NewBestBonusMult       (only on a new best, mult 1.0)
threshold = GaugeThresholdStart + GaugeThresholdStep * gaugeDrafts   (40, +10 each)
```

Each time the gauge reaches the threshold a draft is queued for the next
from-0 reset and the threshold rises; a big new best can queue several at
once, and the popups then chain (pick → next popup, game stays paused).
Leftover charge carries over. Only gauge-earned drafts raise the threshold
(the opening draft does not). Numbers
live in `tune::` (`AugmentDef.hpp`) and are tuned by test; the HUD shows
`NEW BEST +X` when the bonus fires.

Rationale (2026-09-17): the old flat floor (`max(10, percent)`) made
"die at 0 % ten times" the fastest route to a draft. Without the floor a 3 %
death is worth 3, so farming never pays, while being stuck at 60 % still pays
60 per death. The new-best bonus rewards GD's core achievement (new ground is
paid twice), and its total over a run is bounded by `100 * mult`, so it cannot
be farmed either. The rising threshold stops late-run draft floods (stuck at
80 % used to mean a draft every two deaths). Rejected: a decaying floor
(the floor was dropped instead).

**Debug mode** (`debug-mode` setting, default off): number keys 1–9 grant
augments (Shift+1–9 the 10th onwards, i.e. Shift+1 = cat, Shift+2 = brake), key 0 tops the gauge up to the threshold (the draft still happens
on the next death), and `debug-threshold` replaces the ramp with a fixed
cost so one number can be tuned in the settings UI without rebuilding.

## Draft

Three random augments that are not yet maxed, shown on respawn. Picking is
mandatory (no close button, back key ignored). Duplicate picks level the
augment up. Pool is 11 augments, all implemented. Owning `draft-count` raises
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
| `startpos` | 스타트포스 | 5 | 매 어템마다 Z를 눌러 체크포인트를 찍을 수 있습니다. 해당 어템에 죽으면 체크포인트에서 부활합니다. | 체크포인트를 한 번 더 찍을 수 있습니다. | working (v3 verified 2026-09-17). `level` placements per attempt (a life from 0 %), but **only the newest one is live**: placing again moves it; a death respawns there **once**, and the next death restarts from 0 unless a new one was placed in between (budget permitting). The older "each checkpoint is one respawn, newest first" chain was dropped as too loose (user, 2026-09-17). A checkpoint also **snapshots shield charges and brake seconds left** and the respawn restores them (`Augment::onCheckpointPlaced` / `onCheckpointRespawn`). |
| `foresight` | 사륜안 | 1 | 히트박스를 보여줍니다. | – | working. GD colours: blue solid, red hazard, green interactive, yellow player. |
| `unmirror` | 멀미약 | 1 | 레벨 내 모든 미러포탈을 제거합니다. | – | rewritten 2026-09-17 as a `GJBaseGameLayer::toggleFlipped` hook (refs pattern), untested: flips are refused while owned, drafting un-flips at once. |
| `hazard-hitbox` | 위협제거 | 5 | 위험 요소(빨간 히트박스)의 크기가 5% 감소합니다. | 위험 요소의 크기가 5% 더 감소합니다. | built, in-game test pending. Hazard / AnimatedHazard hitboxes shrink around their centre to 1 − 0.05·level (95…75 %); solids, slopes, player untouched. |
| `wave-hitbox` | 웨이브브레이커 | 5 | 웨이브 모드일 때 플레이어 히트박스 크기가 10% 감소합니다. | 웨이브 모드일 때 플레이어 히트박스 크기가 10% 더 감소합니다. | working (verified 2026-09-17). Player rect × (1 − 0.10·level) while `m_isDart`; checked per `PlayerObject`, so in dual only the half that is in wave shrinks. Rotated hazards use the player OBB, which this does not touch (`GD-INTERNALS.md`). |
| `nerve` | 청심환 | 2 | 레벨 후반에 도달할수록 [위협제거]와 [웨이브브레이커]의 효과가 증가합니다. X% 도달 시 두 능력의 효과가 각각 X% 증가합니다. | X% 도달 시 두 능력의 효과가 각각 1.5X% 증가합니다. | working (verified 2026-09-17). Both *shrinks* × (1 + k·progress), k = 1.0 at Lv1 / 1.5 at Lv2 (2X was judged too strong, 2026-09-17); progress = current percent / 100. Either scale is floored at `tune::MinHitboxScale` (0.2), which wave-hitbox Lv5 + nerve Lv2 would otherwise blow past. |
| `draft-count` | 기회비용 | 1 | 다음 드래프트부터 카드가 4개씩 등장합니다. | – | working (verified 2026-09-17). `rollDraft(4)` from the next draft on; the popup narrows the cards to fit. |
| `cat` | 고양이 | 5 | 마법 고양이를 소환합니다. 고양이는 4초마다 시야에 있는 장애물 5개를 랜덤으로 제거합니다. | 고양이가 매번 장애물을 한 개 더 제거하고, 제거 쿨타임이 0.5초 감소합니다. | built 2026-09-17, in-game test pending. Every `4 − 0.5·(lv−1)` s of play, `5 + (lv−1)` random **hazards** (Hazard / AnimatedHazard — solids and slopes are never "장애물" here, removing them would break routes) that are on screen *and ahead of the player* are removed for the rest of the attempt (sprite + hitbox, via GD's `destroyObject()` flags). Restored on every reset. A placeholder square sits bottom-right and fires a laser at each removed hazard; real cat art later. |
| `brake` | 브레이크 | 3 | C를 누르고 있으면 게임 속도가 60% 감소합니다. 어템마다 최대 7초씩 사용할 수 있습니다. | [브레이크]를 어템마다 7초 더 사용할 수 있습니다. | working (verified 2026-09-17). Held key (C, `keybind-brake`): game + music at **40 %** while held, regardless of slow-mo (the cut is absolute, decided with the user 2026-09-17); budget `7·level` **real** seconds per attempt (a from-0 reset refills; a checkpoint respawn restores the seconds left when the checkpoint was placed), a mid-attempt level-up adds its 7 s at once. `BRAKE EMPTY` notice when used up. Pause forgets the held key (focus loss sends no release). Implemented as a time *override* layer in `Scales.cpp` that wins over the slow-mo base speed. |

Definitions and tuning constants live in `src/core/AugmentDef.*` (the
description text quotes the numbers as literals, so change both together);
behaviour in `src/augments/` (one file per augment, see `Augment.hpp`). Debug keys 1-9 grant augments in
table order, Shift+1-9 continue from the 10th (Shift+1 cat, Shift+2 brake).

## Text & fonts (decided 2026-09-17)

In-game augment text is Korean; logs, notices and the HUD's non-name words
stay English. GD's fonts have no Hangul, so the mod ships its own
(`resources/fonts/`). Player-facing UI (draft cards, popup title, centre
notices) uses 아임크리수진 (`ImcreSoojin.ttf`) rendered GD-style — white
glyphs, black outline, drop shadow — baked by `scripts/fontgen.py`. Debug
readouts (the HUD lines) stay in plain Pretendard Regular, generated by Geode.
Both get their charset from the sources (`scripts/fontcharset.ps1`); see
`docs/GD-INTERNALS.md` "Fonts".

## Draft card look (decided 2026-09-17)

GD button styling: white rim, green body with black ring, darker footer band;
top to bottom — name, image box (empty frame until art exists), description,
footer with `NEW` / `Lv a → b` on the left and level pips (gold ★ reached,
grey ☆ remaining) on the right. Cards fan out from the centre when the draft
opens and can't be picked until they land.

## HUD (v2, 2026-09-17)

Kept small so it stays out of the way. The **draft gauge** is a twin of
GD's progress bar mirrored to the bottom edge (same sprite and scale, fill
in the draft cards' green, eased) with `charge/cost` (e.g. `23/40`) in GD's
percent font at its right end; while a gauge-earned draft waits it reads
`40/40` — the free opening draft does not fill it. At the far left, a grey
header (deaths, live %, best %) and then one **row per owned augment**, top
to bottom: a framed placeholder box where the icon will go, the Korean name,
and its English per-attempt state. GD's own progress bar gets rimmed dots:
white at the run's best (moves with the player past it), green at the live
checkpoint (`ProgressMarks`). Decided with the user over four rounds on
2026-09-17. The two hitbox lines
show the scale *at the player's current position*, so they move as nerve ramps
up; 웨이브브레이커 adds `ACTIVE` while a player is in wave. Centre notices for events (SHIELD BROKEN, CHECKPOINT
PLACED, SLOW-MO ON/OFF…).

## Not decided yet

- Draft gauge numbers (40 / +10 / bonus ×1.0) are first guesses; tune by test.
- Whether augmented clears should be recorded as GD clears.
- Remaining augments and systems: candidates, difficulty tiers, rejected ideas
  and build order are in `ROADMAP.md` (2026-09-16).
- Run persistence across game restarts (currently in-memory only).

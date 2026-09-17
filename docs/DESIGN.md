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

## Draft gauge (decided 2026-09-16)

Each death charges the gauge by `max(min-charge, percent reached)`. When it
reaches `draft-threshold`, a draft is queued; leftover charge carries over.
At most one draft per death. Both numbers are mod settings (defaults 10 / 100;
use ~30 for fast test loops).

Rationale: pure death-count rewards nothing for progress; pure new-best stalls
exactly when the player is stuck. This guarantees drafts when stuck and speeds
them up when progressing. Future tuning idea: raise the threshold per draft.

## Draft

Three random augments that are not yet maxed, shown on respawn. Picking is
mandatory (no close button, back key ignored). Duplicate picks level the
augment up. Pool is 9 augments, all implemented. Owning `draft-count` raises
the card count to 4 from the next draft on; the popup then narrows the cards
and scales their text by the same ratio so four still fit GD's 569 pt width.

## Augments (design table, applied 2026-09-17)

Names and descriptions are Korean and shown verbatim on the cards: the
*initial* text when drafting level 1, the *level-up* text for every later
level. The id is the "코드" column and is what the hooks key on.

| id | name | max | initial | level-up | status |
|---|---|---|---|---|---|
| `shield` | 결계인가? | 3 | 매 어템마다 보호막이 지급됩니다. 보호막이 깨지면 1.5초간 노클립 상태로 전환됩니다. | 보호막 개수가 하나 늘어납니다. | working. Covers both players in dual. |
| `slow-mo` | 나무늘보 | 3 | 게임 속도가 5% 감소합니다. X를 눌러 토글할 수 있습니다. | 게임 속도가 5% 더 감소합니다. | working. Speed = 1 − 0.05·level (95/90/85 %), game + music; toggle state persists within the run; pause menu at normal speed. |
| `startpos` | 스타트포스 | 5 | 매 어템마다 Z를 눌러 체크포인트를 찍을 수 있습니다. 해당 어템에 죽으면 체크포인트에서 부활합니다. | 체크포인트를 한 번 더 찍을 수 있습니다. | working. `level` placements per attempt (a life from 0 %); **each placed checkpoint is one respawn**, newest first; with none left the next death restarts from 0 and the budget refills. |
| `foresight` | 사륜안 | 1 | 히트박스를 보여줍니다. | – | working. GD colours: blue solid, red hazard, green interactive, yellow player. |
| `unmirror` | 멀미약 | 1 | 레벨 내 모든 미러포탈을 제거합니다. | – | working. Also neutralises portals already loaded when drafted. |
| `hazard-hitbox` | 위협제거 | 5 | 위험 요소(빨간 히트박스)의 크기가 5% 감소합니다. | 위험 요소의 크기가 5% 더 감소합니다. | built, in-game test pending. Hazard / AnimatedHazard hitboxes shrink around their centre to 1 − 0.05·level (95…75 %); solids, slopes, player untouched. |
| `wave-hitbox` | 웨이브브레이커 | 5 | 웨이브 모드일 때 플레이어 히트박스 크기가 10% 감소합니다. | 웨이브 모드일 때 플레이어 히트박스 크기가 10% 더 감소합니다. | built 2026-09-17, in-game test pending. Player rect × (1 − 0.10·level) while `m_isDart`; checked per `PlayerObject`, so in dual only the half that is in wave shrinks. Rotated hazards use the player OBB, which this does not touch (`GD-INTERNALS.md`). |
| `nerve` | 청심환 | 2 | 레벨 후반에 도달할수록 [위협제거]와 [웨이브브레이커]의 효과가 증가합니다. X% 도달 시 두 능력의 효과가 각각 X% 증가합니다. | X% 도달 시 두 능력의 효과가 각각 1.5X% 증가합니다. | built 2026-09-17, in-game test pending. Both *shrinks* × (1 + k·progress), k = 1.0 at Lv1 / 1.5 at Lv2 (2X was judged too strong, 2026-09-17); progress = current percent / 100. Either scale is floored at `tune::MinHitboxScale` (0.2), which wave-hitbox Lv5 + nerve Lv2 would otherwise blow past. |
| `draft-count` | 기회비용 | 1 | 다음 드래프트부터 카드가 4개씩 등장합니다. | – | built 2026-09-17, in-game test pending. `rollDraft(4)` from the next draft on; the popup narrows the cards to fit. |

Definitions and tuning constants live in `src/core/AugmentDef.*` (the
description text quotes the numbers as literals, so change both together);
behaviour in `src/hooks/PlayLayerHook.cpp`. Debug keys 1-9 grant augments in
table order.

## Text & fonts (decided 2026-09-17)

In-game augment text is Korean; logs, notices and the HUD's non-name words
stay English. GD's fonts have no Hangul, so the mod ships Pretendard
(OFL, `resources/fonts/`): SemiBold for names / notices / the popup title,
Regular for descriptions and HUD lines. Geode converts them to bitmap fonts
at build time with a charset generated from the sources
(`scripts/fontcharset.ps1`), see `docs/GD-INTERNALS.md` "Fonts".

## HUD

Top-left: gauge, deaths, live %, best %, then one line per owned augment
(Korean name, English state) with its per-attempt state. The two hitbox lines
show the scale *at the player's current position*, so they move as nerve ramps
up; 웨이브브레이커 adds `ACTIVE` while a player is in wave. Centre notices for events (SHIELD BROKEN, CHECKPOINT
PLACED, SLOW-MO ON/OFF…).

## Not decided yet

- Real draft trigger tuning (numbers above are placeholders).
- Whether augmented clears should be recorded as GD clears.
- Remaining augments and systems: candidates, difficulty tiers, rejected ideas
  and build order are in `ROADMAP.md` (2026-09-16).
- Run persistence across game restarts (currently in-memory only).

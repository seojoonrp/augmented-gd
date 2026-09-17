# 증강 시각화 브레인스토밍 (2026-09-17)

"숫자만 바뀌는" 증강에 눈에 보이는 피드백을 붙이기 위한 아이디어 모음.
구현 전 단계. 각 항목에 **비용**(프레임당 부담)과 **티어**(ROADMAP 기준,
검증 안 된 GD 사실의 개수)를 적었다. 바인딩은 전부 `bro.ps1`로 확인했고
(2026-09-17), 레퍼런스가 있는 것은 파일:줄을 적었다.

---

## 0. 성능 예산 — 무엇이 싸고 무엇이 비싼가

| 싸다 (프레임당 거의 0) | 비싸다 (피할 것) |
|---|---|
| `m_uiLayer` / `m_objectLayer`에 스프라이트·라벨 몇십 개 두고 액션(`CCScaleTo`, `CCFadeOut`, `CCSequence`)으로 움직이기 | `m_objects` 전체(1만+)를 매 프레임 순회 — 이미 정렬 인덱스 + `lower_bound`로 화면 근처만 보는 패턴이 검증돼 있음(사륜안) |
| `CCDrawNode` 하나를 매 프레임 `clear()` 후 화면 안 도형 ≤ 200개 다시 그리기 (사륜안이 지금 하는 일) | 오브젝트마다 노드 하나씩 붙이기, 오브젝트마다 `setColor` 매 프레임 |
| 파티클 시스템 1~3개, 살아있는 파티클 총 ≤ 150 (GD 자체가 포탈·착지마다 수십 개씩 돌림) | 파티클 수백 개 상시 유지, 매 이벤트마다 `create` 후 안 지우기 |
| GD가 이미 쓰는 링 이펙트 `CCCircleWave`(1개 = 드로우콜 1) | 풀스크린 셰이더 패스 새로 만들기 (레벨에 셰이더 트리거가 없으면 `m_shaderLayer`가 없을 수 있고, 만들면 RTT 한 번 추가) |
| 플레이어 색/글로우/트레일 필드 바꾸기 (이벤트 시점 1회) | 위험 오브젝트 스프라이트 **스케일** 바꾸기 — `m_scaleX/Y`가 `getObjectRadius()`에 들어가서 판정이 같이 변함. 절대 금지 |

규칙:
1. **이벤트 구동 우선** — 죽음·픽·리셋·보호막 소모 같은 순간에만 노드를 만들고,
   상시 효과는 "이미 있는 노드의 필드 하나"만 바꾼다.
2. 프레임당 작업은 전부 `postUpdate` 한 곳에서, 화면 근처 오브젝트만.
3. 노드는 생성/삭제 대신 **재사용 + `setVisible`**. 장식 노드는 컨테이너 하나 밑에
   모아서 `onQuit`에서 통째로 정리.
4. 드래프트 중엔 디렉터가 멈춰 있으므로 **팝업 안 애니메이션은 `visit()` 시계
   패턴**(이미 카드 펼침에 사용). 인게임 효과는 액션 써도 됨.
5. 디버그 HUD에 **프레임 시간(ms)** 한 줄 추가해서 효과 넣기 전/후를 비교.
   "렉 안 걸림"을 감이 아니라 숫자로.

---

## 1. 어떤 순간에 보여줄 것인가 (피드백 매트릭스)

증강마다 다섯 순간이 있고, 지금은 전부 HUD 텍스트 한 줄이다.

| 순간 | 지금 | 목표 |
|---|---|---|
| **픽** (카드 선택) | 팝업 닫힘 | 카드가 HUD 아이콘으로 날아가 자리 잡음 + 효과음 |
| **어템 시작** | – | 그 어템의 자원(보호막 N, 체크포인트 N)이 플레이어 주변에 잠깐 나타났다 사라짐 |
| **발동** (보호막 소모, 체크포인트 부활, 슬로모 토글…) | 가운데 텍스트 | 월드 이펙트(링/파티클/플래시) + 살짝 흔들림 + 효과음 |
| **상시** (히트박스 축소, 슬로모 ON, 청심환 램프) | 텍스트 숫자 | 플레이어 아우라/트레일/외곽선처럼 "몸에 붙은" 표현 |
| **죽음 → 게이지** | 로그 숫자 | 죽은 자리에서 `+37` 떠올라 게이지로 날아가 채움, 임계치 넘으면 "DRAFT READY" |

---

## 2. 시스템 레벨 (증강과 무관하게 런 자체의 맛)

### 2.1 게이지 주스 — 최우선
- 게이지를 텍스트가 아닌 **바**로: `GJ_progressBar_001.png`(GD 프로그레스바
  스프라이트) + 채움 스프라이트를 `setTextureRect`로 잘라 쓰는 GD 자체 방식
  (`m_progressFill`이 그렇게 되어 있음). 노드 2개, 비용 0.
- 죽으면 죽은 위치(월드)에 `+37` 라벨이 떠오르고(`CCMoveBy` + `CCFadeOut`),
  잠시 뒤 HUD 게이지 쪽으로 날아가면서 바가 **이징**으로 찬다. NEW BEST 보너스는
  금색으로 따로 한 번 더 (`+37` 다음 `+12 NEW BEST`).
- 임계치 도달 시 바가 번쩍이고 "DRAFT READY" 뱃지가 붙는다 → 다음 죽음이
  드래프트임을 미리 앎. 지금은 팝업이 떠야 안다.
- 리셋 순간 게이지가 비워지는 게 아니라 **초과분이 남는 것**이 보이게 (바에
  남은 양이 그대로).
- 효과음: 죽음 충전 `counter003.ogg`, 임계치 `highscoreGet02.ogg`,
  드래프트 열림 `chestOpen01.ogg`, 픽 `buyItem01.ogg` / `reward01.ogg`
  (`FMODAudioEngine::playEffect(path)` win ok). 전부 GD 리소스라 추가 파일 없음.
- 티어 T1. 전부 `m_uiLayer` 스프라이트 + 액션.

### 2.2 진행바 장식 — 런의 역사가 보이는 곳
- `PlayLayer::m_progressBar`(CCSprite)에 자식 스프라이트를 붙여:
  - **베스트 마커** (금색 작은 삼각형): 새 베스트가 나올 때 미끄러져 이동.
  - **죽음 눈금** (얇은 빨간 선, 겹칠수록 진해짐): 어디가 벽인지 히트맵이 됨.
    ROADMAP의 Death Marks Lv2.
  - **체크포인트 눈금** (초록): 스타트포스 마커.
- 바 좌표계는 cocos 전용이라 GD 사실 불필요. 티어 T2(눈금 위치 = 바 너비 ×
  percent가 맞는지 한 번만 확인).

### 2.3 월드 죽음 표식 (Death Marks Lv1)
- 죽은 자리마다 작은 X 스프라이트를 `m_objectLayer`에 (`GJ_deleteIcon_001.png`
  프레임 또는 자체 png). 런당 수십 개면 비용 없음. 같은 자리 반복 사망 →
  스케일/투명도 올라감.
- 사륜안이 있으면 그 X에서 "그때 맞은 히트박스"까지 선을 그어주는 확장 가능.

### 2.4 HUD 아이콘 줄 (텍스트 줄 → 아이콘)
- 좌상단 텍스트 줄을 **아이콘 + 레벨 별**로 교체. 아이콘은 카드 이미지 박스와
  같은 그림을 재사용 (어차피 카드 아트가 필요함).
- **활성 상태** 표현: 보호막 아이콘 = 남은 개수만큼 겹침, 소모되면 금 간 버전으로
  교체; 슬로모 ON = 아이콘이 천천히 회전; 체크포인트 = 깃발 개수; 청심환 =
  진행률에 따라 아이콘 밝아짐; 웨이브브레이커 = 웨이브 중일 때만 점등.
- 텍스트 수치(95%)는 아이콘 밑에 작게 `fonts::Debug`로 남기거나 디버그 모드에서만.
- 티어 T1 (아트 필요).

### 2.5 픽 → HUD 애니메이션
- 고른 카드가 축소되며 HUD 아이콘 줄의 빈자리로 날아간다. 디렉터가 멈춘 상태라
  `visit()` 시계로 0.4 s 움직인 뒤 팝업을 닫고 디렉터 재개. 레벨업이면 기존
  아이콘이 튀면서 별이 하나 켜짐.
- 카드 뒷면 → 앞면 **플립**으로 등장(현재는 펼침만). 기회비용으로 4장이 되면 4번째
  카드가 옆에서 슬라이드인 + 금색 테두리로 "보너스 카드"임을 표시.

### 2.6 "강해졌다"가 몸에 보이기
- 총 증강 레벨 합에 따라 플레이어 **글로우 색**이 단계별로 변함
  (`PlayerObject::setGlowColor` / `m_hasGlow`, win ok). 0~2: 기본, 3~5: 시안,
  6+: 금색. GD가 리스폰/색 트리거 때 다시 칠하면 `resetLevel`에서 재적용.
  **(unverified)** — GD가 프레임마다 덮어쓰면 아우라 스프라이트로 대체.
- 아우라 스프라이트: `circle.png`(GD 리소스에 있음)를 additive 블렌드로 플레이어
  자식으로 붙여 약하게 맥동. 자식으로 붙이면 위치 동기화 비용 0. 색 = 위 단계.

### 2.7 런 시작 / 어템 시작 / 클리어
- 런 시작: "RUN START" 배너 + 첫 드래프트 팝업 전에 0.5 s 페이드 (지금은 첫
  프레임 위에 바로 뜸).
- 어템 시작(리셋 직후): 플레이어 주변에 그 어템의 자원 아이콘(보호막 링 N개,
  깃발 N개)이 0.6 s 떠 있다가 사라짐 → "이번 어템에 뭐가 있지"를 매번 알려줌.
- 클리어: 런 요약 팝업 (죽음 수, 드래프트 수, 증강 목록, 시간) — ROADMAP에 있음.
  감정적 마무리. `levelComplete01.plist` 파티클을 배경에.

---

## 3. 증강별 아이디어 (9개)

각 증강에 **상시 / 발동 / 픽** 세 층으로. ★ = 추천 첫 후보.

### 3.1 결계인가? (shield)
- ★ **상시 — 아우라 링**: 남은 보호막 수만큼 얇은 링이 플레이어 주위를 **공전**
  (부모 노드 하나를 `CCRotateBy` 무한, 링은 자식). 3개면 120° 간격. 소모되면
  하나가 깨져 사라짐. `circle.png` 또는 `CCDrawNode` 원 한 개. 비용 0.
  단순 버전: 링 하나, 색으로 개수(파랑/금/흰).
- ★ **발동 — 깨짐**: 소모 순간
  1. `CCCircleWave::create(10, 120, 0.4, false, true)`를 플레이어 위치에
     (GD 포탈/스폰 링과 같은 물건, `refs/qolmod/src/Utils/Utils.cpp:54`),
  2. `burstEffect.plist` 파티클 1발 (`CCParticleSystemQuad::create(plist, false)`,
     `setAutoRemoveOnFinish(true)`),
  3. `shakeCamera(0.15f, 2.f, 0.05f)` 아주 약하게 (`GJBaseGameLayer::shakeCamera`
     win 0x23bc50),
  4. `explode_11.ogg`.
- **노클립 1.5 s 표현**: 플레이어 반투명(`setOpacity(140)`) + **고스트 트레일**
  (`GhostTrailEffect`, qolmod `EditorGhostTrail.cpp:45-51`이 만드는 법 그대로:
  `create`, `m_playerScale`, `doBlendAdditive`, `runWithTarget(m_iconSprite, …)`)
  + 플레이어 주위 **남은 시간 원호** (`CCDrawNode`로 1.5 s에 걸쳐 줄어드는 아크).
  끝나기 0.3 s 전 깜빡임 → "곧 다시 죽는다" 경고.
- **통과한 위험 표시**: 우리가 삼킨 `destroyPlayer(player, object)`의 `object`가
  바로 그 위험 오브젝트 → 그 자리에 작은 스파크(파티클 5개)나 흰 플래시
  스프라이트. "방금 이걸 뚫었다"가 보임. 비용 0 (이벤트당 1회).
- **어템 시작**: 링이 밖에서 안으로 수축하며 장착 (`CCCircleWave` fadeIn=true).
- **픽 / 레벨업**: 링 하나 추가되는 애니메이션을 HUD 아이콘에서.
- 티어: 링/아크/플래시 T1, CircleWave/파티클/고스트 T2 (cocos + GD 클래스, 레퍼런스
  있음), 카메라 흔들림 T2.

### 3.2 나무늘보 (slow-mo)
- ★ **상시 — 화면 톤**: `m_uiLayer`에 풀스크린 `square.png` 하나를 옅은 파랑
  additive(또는 가장자리만 어두운 비네트 png) opacity 30. ON/OFF 시 페이드.
  풀스크린 스프라이트 1장 = 드로우콜 1, 사실상 무료. 비네트가 "시간이 늘어진"
  느낌을 제일 싸게 준다.
- **상시 — 트레일**: ON 동안 고스트 트레일(3.1과 같은 물건, 색만 다르게) 또는
  `m_regularTrail`(CCMotionStreak) 스트로크/페이드 늘리기
  (`refs/qolmod/src/Hacks/Cosmetic/ForceTrail.cpp:51-70`, `LongerTrail.cpp`).
  스케줄러 dt가 이미 줄어 있어 파티클·트레일은 자동으로 느려짐 — 공짜 연출.
- **발동 — 토글**: X 누르면 짧은 `CCCircleWave` 역방향(안으로) + 톤 페이드 + HUD
  시계 아이콘이 느리게/정상으로 돈다. 음악 피치는 이미 신호를 줌.
- **셰이더 옵션(나중)**: `ShaderLayer::triggerColorChange` / `triggerLensCircle`
  (win ok)로 GD 자체 후처리. 레벨에 셰이더 트리거가 없으면 `m_shaderLayer`가 있는지
  불명, 있어도 RTT 비용. **T3, 측정 후 결정**. qolmod는 끄기만 함
  (`NoShaders.cpp`).
- **픽**: 카드 → 아이콘, 레벨 별.

### 3.3 스타트포스 (startpos)
- ★ **상시 — 마커**: 찍은 위치에 **세로 빛기둥**(얇고 긴 그라디언트 png, additive,
  화면 높이만큼) + 밑에 깃발 스프라이트 + 번호(부활 순서). 최신 것이 제일 밝고,
  소모되면 회색 → 부서짐(`glassDestroy01.plist` 또는 페이드). `m_objectLayer`에
  마커당 노드 2개, 어템당 최대 5개.
  GD가 연습모드 마커(`m_physicalCheckpointObject`)를 우리 래퍼에서도 그리는지
  **(unverified)** — 그리면 그걸 쓰고 번호만 얹기.
- **발동 — 찍기**: 플레이어 발밑에서 링 하나 위로 + `crystal01.ogg`. GD 연습모드
  찍기 사운드와 구분되도록 다른 소리.
- **발동 — 부활**: 마커 빛기둥이 순간 강해지며 플레이어가 그 자리에서 나타남 +
  화면 흰 플래시 0.1 s (uiLayer 스프라이트 opacity 180 → 0). `playSpawnEffect()`는
  GD가 이미 호출할 것으로 보임 **(unverified)**.
- **진행바**: 2.2의 초록 눈금.
- **HUD**: 깃발 아이콘 N개 (남은 배치 수) — 텍스트보다 즉각적.
- 티어 T1 (마커·플래시), 부활 타이밍 훅은 이미 있음(`resetLevel`의 fromCheckpoint).

### 3.4 사륜안 (foresight)
이미 시각 증강이지만 더 "능력"처럼:
- **거리 페이드**: 플레이어에서 멀수록 히트박스 alpha 낮게 → 앞쪽만 선명. 지금
  인덱스로 이미 근처만 그리니 alpha 계산만 추가. 비용 0.
- ★ **다음 위험 강조**: 플레이어 앞 가장 가까운 위험(hazard) 1~3개는 두꺼운
  외곽선 + 살짝 맥동. "저게 다음이다"를 짚어줌.
- **플레이어 히트박스**를 노랑 링 글로우로 (지금은 사각형). 웨이브면 3.7과 연결.
- **연출**: 어템 시작에 눈 아이콘이 "떠지는" 0.3 s (HUD 아이콘 스케일 Y 0→1),
  히트박스가 플레이어에서 바깥으로 **스캔되듯** 순서대로 나타남 (거리별 딜레이,
  첫 0.5 s만).
- **죽음 리플레이 힌트**: 죽은 순간 맞은 오브젝트의 히트박스만 0.5 s 빨갛게
  번쩍 (죽음 연출 중에도 `m_objectLayer`는 그려짐). `destroyPlayer`의 `object`
  인자를 그대로 씀. 사륜안 없어도 켜도 될 만큼 싸다.
- 티어 T1 전부.

### 3.5 멀미약 (unmirror)
- ★ **상시 — 무력화 표시**: 미러 포탈에 회색 톤(`setObjectColor` win ok — GD 색
  채널이 다시 덮는지 **(unverified)**; 덮으면 대신 포탈 위에 반투명 X 스프라이트를
  `m_objectLayer`에 얹음) + 금지 표시. 레벨당 포탈 몇 개뿐이라 비용 0.
- **발동 — 통과**: 원래 뒤집혔을 자리에서 "MIRROR NULLIFIED" 알림 + 포탈 자리에
  `portalEffect0X.plist`를 **회색**으로 1발 + 유리 깨지는 소리.
  `toggleFlipped` 훅으로 전환하면 훅 안이 정확히 그 순간.
- **픽**: 픽 직후 화면에 있는 미러 포탈들이 일제히 회색으로 페이드 (이미 로드된
  포탈 처리 시점).
- 티어 T1~T2.

### 3.6 위협제거 (hazard-hitbox) — "숫자만 바뀜"의 대표
사용자가 보고 싶은 건 **줄어든 만큼**이다. 스프라이트 스케일은 금지(0절)이니
판정 자체를 보여준다:
- ★ **안전 마진 밴드**: 화면 근처 위험 오브젝트마다 **원래 히트박스(옅은 빨강)**와
  **축소된 히트박스(진한 빨강)** 사이를 반투명 띠로 그림. 사륜안 인덱스와 draw
  node 재사용, 위험만 그리니 사륜안보다 싸다. 사륜안 없이도 항상 켜지되 alpha 낮게,
  플레이어 근처만. "5%씩 다섯 번"이 눈에 쌓임.
  원래 rect는 `HazardHitboxHook`이 축소 전 값을 알고 있음 → 오브젝트 포인터 →
  원래 크기 맵(또는 scale로 역산: 중심 고정 축소라 `rect / scale`).
- **픽 순간 수축 애니메이션**: 카드 고른 직후 화면의 모든 위험 외곽선이 이전 크기
  → 새 크기로 0.5 s 동안 줄어듦 (draw node에 보간값 하나). 디렉터 재개 직후 실행.
  "방금 작아졌다"를 제일 직접적으로 보여주는 순간.
- **안전 코어 글로우 (대안)**: 위험 중심에 축소 rect 크기의 부드러운 원을 additive로
  — 위협이 "속만 남은" 느낌. 같은 draw node, 색만 다름. 밴드와 택1.
- **청심환 연동**: 밴드 색이 진행률에 따라 주황 → 시안으로 (3.8).
- **HUD**: 스파이크 아이콘 위에 축소 % 링.
- 티어 T1 (전부 이미 있는 인프라).

### 3.7 웨이브브레이커 (wave-hitbox)
- ★ **트레일 얇게**: `m_waveTrail->m_waveSize`를 축소 비율만큼 줄인다 —
  qolmod `WaveTrailSize.cpp:44-49`가 정확히 이 필드를 씀. 필드 하나 = 비용 0,
  그리고 "웨이브가 작아졌다"를 GD 자체 그림으로 보여줌. **가장 가성비 좋은 항목.**
- **웨이브 진입 순간**: `m_isDart`가 false→true 되는 프레임에 작은 링 + "WAVEBREAKER"
  알림 (지금 HUD `ACTIVE`를 알림으로 승격). 진입은 `postUpdate`에서 플래그 엣지 검출.
- **플레이어 히트박스 외곽선**: 웨이브 중엔 축소된 플레이어 rect를 노랑으로 항상
  표시 (사륜안 없어도). 원래 크기는 옅게 → 3.6과 같은 "마진" 언어.
- **레벨업**: 트레일이 한 단계 더 가늘어지는 게 그대로 보임.
- 티어 T2 (필드 접근, 레퍼런스 있음).

### 3.8 청심환 (nerve) — 진행률 램프
- ★ **심박 아우라**: 플레이어 아우라(2.6)가 **심박처럼** 맥동하되, 진행률이 오를수록
  느리고 커진다 (0%: 빠르고 작게, 100%: 느리고 크게) — "마음이 가라앉는" 테마.
  스케일 액션 주기 하나 바꾸는 것뿐.
- **색 램프**: 3.6/3.7 외곽선·밴드·트레일 색이 주황(0%) → 시안(100%)으로 보간.
  청심환이 없으면 색 고정 → 있는지 없는지 화면만 봐도 앎.
- **HUD 집중 게이지**: 진행률 바와 별개로 "효과 배율 ×1.00 → ×2.50"을 원형
  게이지로.
- **비네트 옵션**: 진행률에 따라 가장자리 시안 비네트 강해짐 (3.2 톤 스프라이트와
  같은 노드, 색만).
- **레벨업(×1.5)**: 아우라 색 한 단계 위로.
- 티어 T1.

### 3.9 기회비용 (draft-count)
- 인게임 상시 표현은 필요 없음. 드래프트 팝업에서:
  - 4번째 카드가 **금테**로 슬라이드인, 제목 옆 `+1`.
  - HUD 아이콘 줄에 카드 4장 아이콘.
- 티어 T1.

---

## 4. 기술 카탈로그 (재료별 비용·검증 상태)

| 재료 | API / 레퍼런스 | 비용 | 티어 |
|---|---|---|---|
| 화면 고정 스프라이트·라벨 | `m_uiLayer` addChild (RunHud가 이미 이렇게) | 0 | 검증 |
| 월드 스프라이트 | `m_objectLayer` addChild (사륜안 draw node) | 0 | 검증 |
| 한 draw node에 화면 근처 도형 | 정렬 인덱스 + `lower_bound` (사륜안) | ≤200개면 0 | 검증 |
| 액션 애니메이션 | `CCScaleTo`/`CCFadeOut`/`CCMoveBy`/`CCRotateBy` | 0 | 검증(드래프트 중엔 멈춤 → visit 시계) |
| GD 링 이펙트 | `CCCircleWave::create(start, end, dur, fadeIn, easeOut)` win 0x42870; qolmod `Utils.cpp:54`, `PulsingCircle.cpp` | 드로우콜 1 | T2 |
| GD 파티클 | `CCParticleSystemQuad::create("burstEffect.plist", false)` (cocos, xdbot `renderer.cpp:20`가 같은 시그니처 훅); GD 리소스 plist 목록: burst/explode/charge/land/dash/glitter/portalEffect0X/levelComplete01/glassDestroy01/lvlupEffect… | 파티클 수에 비례, ≤150 | T2 |
| GD 풀 파티클 | `GJBaseGameLayer::claimParticle(key, zLayer)` win 0x2409d0 (GD가 자체 풀에서 꺼내줌) | 0 | T3 (키 문자열·반환 수명 불명) |
| 고스트 트레일 | `GhostTrailEffect::create` + `runWithTarget(m_iconSprite, 0.05, 0.04, 0, 0.6, false)`; qolmod `EditorGhostTrail.cpp:45-51`, `ForceGhost.cpp` | 스냅샷 스프라이트 ~10개 | T2 |
| 일반 트레일 | `m_regularTrail` (CCMotionStreak) `m_bStroke`/`setStroke`/블렌드; qolmod `ForceTrail.cpp`, `LongerTrail.cpp`, `NoTrailBlending.cpp` | 0 | T2 |
| 웨이브 트레일 두께 | `m_waveTrail->m_waveSize`; qolmod `WaveTrailSize.cpp:44-49` | 0 | T2 |
| 플레이어 색/글로우 | `PlayerObject::setColor`/`setSecondColor`/`setGlowColor`/`updateGlowColor` win ok, `m_playerColor1/2`, `m_hasGlow` | 0 | T2 (GD가 언제 다시 칠하는지 unverified) |
| 플레이어 투명도 | `setOpacity` (CCSprite 상속) | 0 | T2 |
| 오브젝트 색 | `GameObject::setObjectColor`/`setGlowColor` win ok, `m_baseColor` | 0 | T3 (색 채널이 매 프레임 덮을 가능성) |
| 카메라 흔들림 | `GJBaseGameLayer::shakeCamera(dur, strength, interval)` win 0x23bc50 | 0 | T2 |
| 진행바 장식 | `PlayLayer::m_progressBar`/`m_progressFill` (CCSprite) 자식 | 0 | T2 (좌표계만 확인) |
| 효과음 | `FMODAudioEngine::sharedEngine()->playEffect("explode_11.ogg")` win 0x56dc0; GD ogg: achievement_01, buyItem01/03, chestOpen01, counter003, crystal01, explode_11, gold01/02, highscoreGet02, magicExplosion, reward01, secretKey… | 0 | T2 |
| GD 후처리 셰이더 | `GJBaseGameLayer::m_shaderLayer` + `ShaderLayer::triggerShockWave/triggerLensCircle/triggerColorChange/triggerChromaticGlitch/triggerInvertColor/triggerPinchX/Y/triggerRadialBlur/triggerBulge` (win ok); grayscale/sepia/hueShift/glitch/pixelate/motionBlur는 win inline(호출은 가능할 수도) | 레이어가 없으면 생성 = 풀스크린 RTT 추가 | T3 — 마지막에, 측정 후 |
| 풀스크린 톤/비네트 | `square.png` 또는 자체 비네트 png 한 장, additive/alpha | 드로우콜 1 | T1 |

---

## 5. 하지 말 것
- 위험 오브젝트 `setScale` — 판정 반경이 같이 변함.
- `m_objects` 전체에 매 프레임 `setColor`/`setOpacity`.
- 오브젝트마다 자식 노드 (배치 노드가 깨지고 노드 수가 만 단위).
- 레벨에 없던 `ShaderLayer`를 상시로 만들어 두기 (측정 전엔).
- `CCDrawNode` 반투명 채움 — premultiplied 알파라 불투명하게 나옴 (검증됨).
  채움은 `{0,0,0,0}`, 반투명은 스프라이트로.
- 드래프트 팝업 안에서 cocos 액션 (디렉터 정지 중).

---

## 6. 추천 순서 (맛 ÷ 노력)

1. **게이지 주스** (2.1) — 루프의 심장. 텍스트 → 바 + 떠오르는 `+N` + DRAFT READY + 효과음. T1.
2. **웨이브 트레일 얇게** (3.7) + **안전 마진 밴드** (3.6) — "숫자만 바뀜" 두 증강이
   바로 보이게. 필드 1개 + draw node 재사용. T1/T2.
3. **보호막 링 + 깨짐 연출** (3.1) — 제일 상징적인 증강, 링/CircleWave/파티클/소리. T1/T2.
4. **체크포인트 빛기둥 + 진행바 눈금** (3.3, 2.2). T1/T2.
5. **슬로모 비네트 + 고스트 트레일** (3.2). T1/T2.
6. **청심환 심박 아우라 + 색 램프** (3.8, 2.6) — 위 것들에 색만 얹으면 됨. T1.
7. **HUD 아이콘 줄 + 픽 날아가기** (2.4, 2.5) — 아트가 생기면. T1.
8. **죽음 표식 / 다음 위험 강조 / 맞은 히트박스 플래시** (2.3, 3.4). T1.
9. **셰이더 효과** (3.2 옵션) — 프레임 시간 HUD로 측정한 뒤에만. T3.

각 단계는 빌드 1회 + 테스트 1회 단위로 끊어지게 설계했고, 1~3만 해도 "보이는
증강"이 된다.

---

## 7. 검증이 필요한 사실 (GD-INTERNALS에 옮길 후보)
- 우리 연습모드 래퍼로 `markCheckpoint()`했을 때 GD가 마커 스프라이트를 그리는가.
- `setGlowColor`/`setObjectColor`를 GD가 언제 다시 덮어쓰는가 (리스폰? 매 프레임?).
- 셰이더 트리거 없는 레벨에서 `m_shaderLayer`가 null인가; 직접 만들면 동작하는가.
- `claimParticle`의 키 문자열과 반환 노드 수명.
- 카드 아트 / 아이콘 파일 형식 (자체 png → `resources.sprites`, `_spr`).

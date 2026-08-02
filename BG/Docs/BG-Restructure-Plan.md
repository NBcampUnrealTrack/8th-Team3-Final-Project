# BG 구조 개편 및 리포지토리 이주 계획

> 대상: `Baam` 프로젝트 → 새 리포지토리
> 선행 문서: `Code-Audit-And-Migration-Plan.md`
> 작성 2026-08-03

## 진행 상황

| Phase | 내용 | 상태 |
|---|---|---|
| 0 | 준비·기준선 | ✅ 태그 `baseline-before-restructure` |
| 1 | 폴더 재편 + Public/Private 강제 | ✅ 클린 리빌드 통과 |
| 2 | 클래스 개명 `Baam*`/`Bang*` → `BG*` | ✅ 애셋 13개 리세이브·검증 완료 |
| 3 | 모듈 개명 `Baam` → `BG` | ✅ 클린 빌드 통과 — **에디터 기동 확인 대기** |
| 4 | 프로젝트 **폴더** 개명 | ✅ Phase 6 과 함께 (`Baam/` → `BG/`) |
| 5 | Content 재편 | ✅ 애셋 16개 이동, `AbilityByCardId` 검증 통과 |
| 6 | 새 리포로 이전 | ✅ 클린 빌드 통과 |

---

## 0. 목표와 결정 사항

| # | 목표 | 결정 |
|---|---|---|
| 1 | `Baam` / `Bang` 접두사를 `BG` 로 통일 | **모듈·프로젝트 파일까지 전부** 개명 |
| 2 | 평평한 파일 배치를 작은 폴더로 분할 | §3 트리 |
| 3 | 헤더는 `Public/`, cpp 는 `Private/` | **분리를 컴파일러로 강제** |

보조 결정
- `GA_` / `GE_` 접두사는 **유지**한다. GAS 전체의 관례이고 BP 애셋명과도 맞는다.
- git 히스토리는 **새로 시작**한다. → **구 리포를 지우지 말 것.** 히스토리를 버리는 대신
  구 리포가 유일한 기록 보관소가 된다.

---

## 1. 실측 — 왜 이 계획이 성립하는가

| 실측 항목 | 값 | 의미 |
|---|--:|---|
| `Private/` 안의 헤더 | **0개** | 목표 3 은 이미 95% 되어 있었다 |
| Public/Private 규칙 위반 폴더 | **1개** (`Source/Baam/UI/`) | 20개 파일만 옮기면 끝 |
| C++ 를 참조하는 애셋 | **13개** | 전수 리세이브가 현실적이다 |
| 무관한 아트 애셋 | 2,873개 (`PolygonFantasyKingdom`) | 개명 영향 **0** |
| 모듈 개명 전례 | `TP_Blank` → `Baam` | `DefaultEngine.ini:253` 에 흔적 |

### 1.1 목표 3 이 싼 이유

`Baam.Build.cs` 의 이 한 줄이 Public/Private 분리를 무력화하고 있었다.

```csharp
PublicIncludePaths.Add(ModuleDirectory);
```

**이 줄을 지워서 깨지는 곳은 `Source/Baam/UI/` 뿐이었다.** UBT 가 `Public/` 과 `Private/` 를
자동으로 include 경로에 넣기 때문에 `#include "Game/BaamGameState.h"` 같은 경로는 이미
`Public/` 루트를 통해 해석되고 있었다. `ModuleDirectory` 는 오직 `Source/Baam/UI/` 라는
제3의 위치를 찾기 위해서만 필요했다.

### 1.2 애셋이 13개뿐이라는 사실이 결정적이다

이 규모의 개명이 보통 무서운 이유는 "수백 개 애셋 중 하나를 빠뜨리면 나중에 조용히
깨진다" 는 점이다. 여기선 전수가 13개라 한 번에 다 열어볼 수 있다.

```
Content/GameSystem/Data/DT_BaamCard.uasset
Content/GameSystem/Data/DT_BaamCharacterRow.uasset
Content/GameSystem/Data/Stat_Character/AT_Sheriff.uasset
Content/GameSystem/Test/BP_TestGamemode.uasset
Content/GameSystem/Test/BP_TestPlayerController.uasset      ← AbilityByCardId 가 여기 있다
Content/Network/UI/WBP_BaamSessionMenu.uasset
Content/UI/WBP_Bang{CardWidget,Hand,HUD,PlayZone,SeatBoard,SeatWidget,TurnPanel}.uasset
```

---

## 2. ⚠️ 절대 건드리면 안 되는 것 — 이 개편의 유일한 진짜 위험

**`Bang` 이라는 글자는 두 가지 서로 다른 것을 가리킨다.**

| 종류 | 예 | 처리 |
|---|---|---|
| 프로젝트의 옛 이름 | `UBangCardWidget`, `FBangSeatView` | → `BG` 로 개명 |
| **게임 안의 카드 이름 (뱅! 카드)** | `Card.Id.Bang`, `Ability.Bang`, `UGA_Bang` | **절대 개명 금지** |

`sed 's/Bang/BG/g'` 를 실행하면 `Card.Id.Bang` 이 `Card.Id.BG` 가 되어 **DT 와 BP 가
문자열로 참조하는 태그가 전부 끊어진다.** 뱅 카드가 죽고, 원인이 코드 어디에도 안 보인다.

### 2.1 손대면 안 되는 문자열

```
Card.Id.*  (31종 전부, Card.Id.Bang / Card.Id.AreaBang 포함)
Ability.Bang     Ability.AreaBang
Resolution.*     Phase.*     Role.*     Card.Type.*     Card.Slot.*
```

이들은 **데이터다.** `DT_BaamCard` 의 Row 와 BP 의 `AbilityByCardId` 키가 이 문자열을
그대로 들고 있다. C++ 에서만 바꾸면 데이터 쪽과 조용히 어긋난다.

### 2.2 개명에서 제외하는 식별자

Phase 2 실행 전 `Bang` 을 포함한 토큰을 전수 조사해 얻은 목록이다.
**처음 계획에는 앞의 둘만 있었다** — 나머지는 조사에서 나왔고, 놓쳤다면
게임 규칙 어트리뷰트가 조용히 깨졌을 것들이다.

| 식별자 | 정체 |
|---|---|
| `UGA_Bang` | "뱅!" 카드의 능력. 프로젝트명이 아니다. |
| `UGA_AreaBang` / `AreaBang` | 광역 뱅(개틀링·인디언) |
| `BangLimit` | **어트리뷰트** — 턴당 뱅 사용 한도 (GDD §7.1) |
| `DrawBangCount` | **어트리뷰트** — 뱅 판정 시 굴리는 횟수 |
| `BangPlayed` | 태그 `State.Turn.BangPlayed` 의 식별자 |
| `OnBangResolved` | 뱅 판정 완료 이벤트 |

### 2.3 다른 단계에 속하는 것 — Phase 2 에서 봉인

| 대상 | 이유 | 처리 단계 |
|---|---|---|
| `DT_Baam*` / `WBP_Baam*` | 애셋 이름. 지금 바꾸면 **없는 애셋을 찾는다** | Phase 5 |
| `/Script/Baam.` | 모듈 경로 | Phase 3 |
| `BAAM_API` | 모듈 API 매크로 (모듈명에서 생성됨) | Phase 3 |
| `Private/Baam.h` `.cpp` | 모듈 구현부 | Phase 3 |

### 2.4 실제로 쓴 치환 규칙

`Baam` 은 게임 개념에 쓰인 적이 없다. **§2.3 을 봉인하고 나면 전역 치환이 안전하다.**

`Bang` 은 그럴 수 없다. 아래 규칙 중 어느 것도 매치하지 않는 것이 곧 보존 대상이 되도록
설계했다 — 별도의 보호 목록 없이 §2.2 가 자동으로 비켜간다.

```python
r"\bnamespace Bang\b"  → "namespace BG"    # 태그 네임스페이스 선언
r"\bBang::"            → "BG::"            # Bang::Card::Id::Bang → BG::Card::Id::Bang
r"\bFOnBang"           → "FOnBG"           # 델리게이트
r"\bFBang" r"\bUBang"  → "FBG" "UBG"
r"\bLogBang"           → "LogBG"
r"\bGBang" r"\bCVarBang" → "GBG" "CVarBG"
r"\bBang(CardView|SeatView|TurnView|CardWidget|HandWidget|PlayZoneWidget|
         SeatWidget|SeatBoardWidget|TurnPanelWidget|CardDragDropOperation)" → r"BG\1"
r'"Bang\.' r'"Bang\|' r'"Bang"' r"\[Bang\]"   # 콘솔변수·카테고리·NSLOCTEXT·로그 접두
```

`GA_Bang` 은 `_` 가 단어 문자라 `\bBang` 이 매치하지 않는다. `BangLimit` 은 접두사 규칙의
접미사 목록에 없다. `AreaBang` · `DrawBangCount` · `OnBangResolved` 는 `Bang` 앞이 문자라
경계가 성립하지 않는다. **전부 규칙의 구조상 자동으로 보존된다.**

```bash
# ❌ 절대 금지
s/Bang/BG/g        s/Baam/BG/g      # 봉인 없이는 Baam 도 애셋 경로를 망가뜨린다
```

### 2.5 `NSLOCTEXT("Bang", ...)` — 지금 바꿔라

10곳에서 로컬라이제이션 네임스페이스로 `"Bang"` 을 쓴다. 지금은 번역 파일이 없어 무해하지만
**번역을 시작한 뒤에 바꾸면 기존 번역이 전부 무효화된다.** 지금이 가장 싼 시점이다.
반드시 전부 한꺼번에 바꾼다.

---

## 3. 폴더 구조 (Phase 1 에서 적용 완료)

### 3.1 설계 원칙

1. **폴더 하나에 헤더 3개 이상.** 파일 1개짜리 폴더는 분류가 아니라 소음이다.
   그래서 `BGGameplayTags.h` 는 `Tag/` 를 만들지 않고 `Public/` 루트에 둔다 —
   모듈 전역 어휘라는 사실이 위치로 드러난다.
2. **`UI/View/` 와 `UI/Widget/` 을 가른다.** 취향이 아니라 **의존 방향을 강제**하기
   위해서다. `BaamPlayerController.h` 가 `BangCardView.h` 를 include 한다. 뷰모델이
   위젯과 섞여 있으면 Core → UMG 의존이 생긴다. 갈라 두면 `View/` 는 아무것도 의존하지
   않는 순수 데이터로 남는다.
3. **비활성 코드는 폴더로 격리한다.** `GAS/Ability/Legacy/` 는 감사 문서 §2.7 의
   "표기가 들쭉날쭉" 문제를 주석이 아니라 경로로 해결한다.

### 3.2 트리 (Phase 2~3 개명 후 기준)

```
Source/BG/
  BG.Build.cs
  Public/
    BGGameplayTags.h                  ← 모듈 전역 어휘. 폴더에 넣으면 오히려 묻힌다.
    Core/      BGGameMode  BGGameState  BGGameInstance
               BGPlayerController  BGPlayerState  BGCharacter
    Card/      BGCardType  BGGameDataTypes  BGDataSubsystem
               BGDataSettings  BGDiceComponent
    GAS/       BGAbilitySystemComponent  BGAttributeSet  BGGameplayAbility
      Ability/         GA_BGCardEffects        ← 카드 효과의 유일한 정식 경로
        Legacy/        GA_Bang  GA_Heal  GA_Missed  GA_DrawN  GA_Duel
                       GA_AreaBang  GA_GeneralStore  GA_Saloon  GA_StealOrDiscard
                       ← [비활성] GDD §13.1. AbilityByCardId 에 매핑 금지.
      Effect/          GE_BGEquip  GE_BGDamage  GE_BGHeal
    Lobby/     BGMatchStartComponent  BGReadyComponent
    Session/   BGSessionFlow  BGSessionInterface  BGSessionTypes  BGOssPolicy
    UI/
      View/    BGCardView  BGSeatView  BGTurnView          ← 순수 데이터. 의존 없음.
               BGCardViewLibrary  BGSeatViewLibrary
      Widget/  BGCardWidget  BGHandWidget  BGPlayZoneWidget
               BGSeatWidget  BGSeatBoardWidget  BGTurnPanelWidget
               BGCardDragDropOperation
      Session/ BGSessionMenuWidget  BGSessionMenuController
    Debug/     BGCardLog  BGNetLog  BGDebug
  Private/
    BG.h  BG.cpp                      ← 모듈 구현부. 아무도 include 하지 않는다.
    <Public 과 동일 트리의 .cpp>
    Debug/  BGCardExec.cpp  BGSessionExec.cpp   ← 헤더 없음. exec 콘솔 명령 전용.
```

---

## 4. 진행 순서 — 폭발 반경이 작은 것부터

**각 단계는 독립적으로 가치가 있고, 독립적으로 되돌릴 수 있다.**
Phase 2 에서 멈춰도 목표 2·3 과 목표 1 의 대부분을 얻는다. 이게 순서의 근거다.

| Phase | 애셋 영향 | 되돌리기 | 검증 |
|---|---|---|---|
| 1 폴더 재편 | **없음** | `git revert` | 컴파일 |
| 2 클래스 개명 | 13개 리세이브 | `git revert` + 리세이브 | 컴파일 + 에디터 |
| 3 모듈 개명 | 13개 리세이브 | 어려움 | 에디터 기동 |
| 4 프로젝트 파일 | 없음 | 쉬움 | 에디터 기동 |
| 5 Content 재편 | 이동만 | 쉬움 | 에디터 |
| 6 새 리포 | — | — | 클린 클론 빌드 |

> **Phase 1·2 를 한 커밋에 합치지 않는다.** 파일 이동과 내용 변경이 섞이면 git 의 rename
> 감지가 실패해 diff 가 "전부 삭제 + 전부 추가" 로 보인다. 컴파일이 깨졌을 때 이동
> 때문인지 개명 때문인지 구분할 수 없게 된다.

---

## Phase 0 — 준비 ✅

```bash
git merge chore/code-audit-cleanup      # 안 하면 진단 가드 수정이 유실된다
# 에디터에서 DT_BaamCard 재임포트
Build.bat BaamEditor Win64 Development  # Result: Succeeded
git tag baseline-before-restructure
```

기준선이 깨진 상태에서 대규모 이동을 하면 이후 모든 실패의 원인을 특정할 수 없다.

---

## Phase 1 — 폴더 재편 + Public/Private 강제 ✅

99개 파일 이동, 76개 파일의 include 수정, 빈 폴더 15개 제거. 애셋은 건드리지 않았다.

`Baam.Build.cs` 에서 `PublicIncludePaths.Add(ModuleDirectory);` 를 삭제하고, 다시
들어오지 못하도록 이유를 주석으로 남겼다.

### 이동 중 발견한 잠복 버그 2건

**1. 같은 폴더라서 통하던 베어 include**

```cpp
// Public/Game/BaamGameState.h:6
#include "BaamCardType.h"        // ← 경로 없음
```

두 파일이 같은 `Public/Game/` 에 있어서 동작했다. Phase 1 에서 `Core/` 와 `Card/` 로
갈라지면 그대로 깨진다. → `#include "Card/BaamCardType.h"` 로 수정.

**2. `.generated.h` 의 대문자 G**

```cpp
#include "BaamCardType.Generated.h"   // ← Windows 밖에서는 못 찾는다
```

파일 시스템이 대소문자를 구분하지 않는 Windows 에서만 통한다. 빌드 서버나 Linux 전용
서버를 붙이는 순간 터진다. → 소문자 `.generated.h` 로 수정.

### 검증

```
클린 리빌드 (Intermediate/Build 삭제 후)  →  Result: Succeeded (74.97s)
```

애셋을 안 건드렸으므로 에디터 확인은 생략했다.

---

## Phase 2 — 클래스 개명 (`Baam*` / `Bang*` → `BG*`) ⚠️ 에디터 검증 대기

파일 79개 개명, 97개 파일 내용 수정, 리다이렉트 51건.
이름을 유지한 것은 정확히 `GAS/Ability/Legacy/` 의 18개(9종 × `.h`/`.cpp`)뿐이다.

### 계획에서 벗어난 점

**1. 순수 접두사 개명만 했다.** 초안의 `BaamGameDataTypes` → `BGCardData`,
`UBaamDataSubsystem` → `UBGCardDataSubsystem` 같은 **의미 개명은 하지 않았다.**
사용자가 요청한 것은 접두사 통일이고, 애셋을 건드리는 바로 그 단계에서 diff 에
의미 변경을 섞으면 검토가 불가능해진다. 의미 개명은 애셋 의존이 거의 없으므로
나중에 따로 하는 편이 싸다.

**2. `UGE_BaamEquip` 는 존재하지 않았다.** §7.1 초안의 오류다. `GE_BaamEquip.h` 안의
클래스는 `UGE_Equip_DwarfGloves` / `UGE_Equip_WitchCharm` 뿐이고 둘 다 아이템 고유
이름이라 개명 대상이 아니다. 파일명만 `GE_BGEquip.h` 로 바꿨다.

**3. 세션 프로토콜 키도 개명했다.** `"BaamGameName"` `"BaamRoomCode"` `"BaamRoomName"`
`"Baam"` 은 `FOnlineSessionSettings` 의 키다. → **구버전 빌드와 신버전 빌드는 서로의
방을 찾지 못한다.** 어차피 Phase 3 의 모듈 개명으로 호환이 끊기므로 함께 처리했다.
팀원 전원이 같은 시점에 갱신해야 한다.

### 리다이렉트 생성에서 두 번 놓칠 뻔한 것

자동 생성 정규식이 `[UAFEIS]Baam` 형태만 잡아서 **`UGA_BaamCardEffects` 가 빠졌다.**
카드 실행의 유일한 경로이자 `AbilityByCardId` 가 가리키는 바로 그 클래스다.
이어서 `UCLASS(meta = (DisplayName = "..."))` 의 중첩 괄호 때문에 `UBaamDataSettings`,
`FBaamOssPolicy` 도 빠졌다.

→ 결국 **매크로 파싱을 포기하고 `class|struct|enum class` 선언 전수에서 뽑았다.**
존재하지 않는 이름의 리다이렉트는 무해하므로, 넓게 잡는 쪽이 항상 옳다.

### 남은 작업 — 에디터에서

1. 에디터 실행 → **애셋 13개를 전부 열어 확인** → `Save All`
2. **`BP_TestPlayerController` 의 `AbilityByCardId` 맵을 눈으로 볼 것.**
   비어 있으면 모든 카드가 죽는다. 카드를 한 장 실제로 내 보는 것이 가장 확실하다.
3. 확인 후 `DefaultEngine.ini` 의 Phase 2 리다이렉트 블록 삭제
4. 에디터 재기동 → 13개 재확인

> 4번을 건너뛰면 "리다이렉트 덕에 열리던 것" 과 "진짜 고쳐진 것" 을 구분할 수 없다.

---

## Phase 2 절차 (참고)

1. §7 매핑표대로 토큰 고정 치환. 파일명도 함께 변경.
2. `#include "X.generated.h"` 는 파일명과 반드시 일치해야 한다 — 파일명을 바꾸면 UHT 가
   생성하는 이름도 바뀐다. basename 기준으로 치환하면 자동으로 맞는다.
3. `Config/DefaultEngine.ini` 에 리다이렉트 추가:
   ```ini
   [CoreRedirects]
   +ClassRedirects=(OldName="/Script/Baam.BaamGameMode",        NewName="/Script/Baam.BGGameMode")
   +ClassRedirects=(OldName="/Script/Baam.BaamPlayerController",NewName="/Script/Baam.BGPlayerController")
   +ClassRedirects=(OldName="/Script/Baam.BangCardWidget",      NewName="/Script/Baam.BGCardWidget")
   ; ... §7 표 전부
   +StructRedirects=(OldName="/Script/Baam.BaamCardRow",        NewName="/Script/Baam.BGCardRow")
   +EnumRedirects=(OldName="/Script/Baam.EBaamDiceOutcome",     NewName="/Script/Baam.EBGDiceOutcome")
   ```
   > 모듈 부분(`/Script/Baam`)은 아직 그대로다. Phase 3 에서 따로 처리한다.
   > **두 개명을 한 리다이렉트에 섞지 않는다** — 실패했을 때 어느 쪽 문제인지 알 수 없다.
4. `Config/DefaultGame.ini` 의 섹션명 `[/Script/Baam.BGDataSettings]`,
   `Config/DefaultEngine.ini:5` 의 `GameInstanceClass=/Script/Baam.BGGameInstance` 수정.
5. 빌드 → 에디터 실행 → **13개 애셋 전수 확인** → `Save All`.
6. **리다이렉트 블록 삭제** → 에디터 재기동 → 13개 재확인.

> 6번의 재기동 확인을 건너뛰면 "리다이렉트 덕에 열리던 것" 과 "진짜 고쳐진 것" 을
> 구분할 수 없다.

### 검증

`BP_TestPlayerController` 의 **`AbilityByCardId` 맵이 살아 있는지 눈으로 볼 것.**
이게 비면 모든 카드가 죽는다. 카드 한 장을 실제로 내 보는 것이 가장 확실하다.

---

## Phase 3 — 모듈 개명 (`Baam` → `BG`) ✅

소스 42개 파일(`BAAM_API` → `BG_API`, `/Script/Baam.` → `/Script/BG.`), 이동 7건.
클린 빌드 `Module.BG.cpp` → `Result: Succeeded`.

### Phase 4 에서 앞당겨 처리한 것

`.uproject` **파일명**도 여기서 함께 바꿨다(`Baam.uproject` → `BG.uproject`).
`IMPLEMENT_PRIMARY_GAME_MODULE(…, BG, "BG")` 의 게임명과 프로젝트 파일명이 어긋난
중간 상태를 만들지 않기 위해서다. Phase 4 에 남은 것은 **폴더 개명뿐**이고,
그것만이 팀원 로컬 경로를 끊는 작업이다.

### 에디터가 리다이렉트를 주석 처리해 두었다

Phase 2 애셋 리세이브 때 **에디터가 `[CoreRedirects]` 의 51줄을 `;+ClassRedirects=…`
로 주석 처리해 저장했다.** 소비를 마쳤다는 표시다. 삭제 스크립트의 정규식이
`^\+` 로 시작해서 이 51줄을 통째로 비껴갔다 — 접두 `;` 를 포함하도록 고쳐 제거했다.

함께 정리한 것
- `TP_Blank` → `Baam` 리다이렉트 2줄 (이 프로젝트의 전전 이름. 역할 끝)
- `+PropertyRedirects=(OldName="BaamPlayerController.AbilityByEventTag"…)`
  — 그 클래스는 이제 어느 모듈에도 없다. `BGPlayerController` 판만 남겼다.

### 대문자라 규칙을 빠져나간 것

`BGGameInstance.cpp` 의 로컬 상수 `BAAM_SESSION_KEY` 등 5개가 남아 있었다.
**값은 `TEXT("BGGameName")` 으로 바뀌었는데 변수 이름만 옛것이었다** —
치환 규칙이 `Baam`(혼합 대소문자)만 봤기 때문이다. `BG_*` 로 정정.

산문 주석의 `BAAM` 4건은 **게임 제목**을 가리키므로 그대로 둔다
(`// BAAM 신규 카드`, `// BAAM 에는 "빗나감" 카드가 없다`).
게임 이름은 BAAM 이고 `BG` 는 코드 접두사일 뿐이다.

### 남은 확인 — 에디터에서

1. `BG.uproject` 로 에디터 기동 (프로젝트 파일 재생성 필요할 수 있음)
2. 애셋 13개 로드 경고 0 확인
3. `BG.ExpectedDeckSize 64` → `BG_DumpDeck` → 경고 0건
4. `git tag restructure-phase3-ok`

---

## Phase 3 절차 (참고)

여기부터 실패하면 에디터가 안 열린다. **Phase 2 가 확실히 통과한 뒤에 시작한다.**

```bash
git mv Source/Baam Source/BG
git mv Source/BG/Baam.Build.cs     Source/BG/BG.Build.cs
git mv Source/Baam.Target.cs       Source/BG.Target.cs
git mv Source/BaamEditor.Target.cs Source/BGEditor.Target.cs
```

수정할 곳
- `BG.Build.cs` — `public class Baam : ModuleRules` → `public class BG`
- `BG.Target.cs` / `BGEditor.Target.cs` — 클래스명, `ExtraModuleNames` 의 `"Baam"` → `"BG"`
- `Private/BG.cpp` — `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, BG, "BG");`
- 전 소스 — `BAAM_API` → `BG_API`
- `Baam.uproject` — `"Name": "BG"`
- `Config/*.ini` — `/Script/Baam.` → `/Script/BG.`

리다이렉트 (이 프로젝트가 `TP_Blank` → `Baam` 때 쓴 것과 같은 방식):
```ini
[/Script/Engine.Engine]
+ActiveGameNameRedirects=(OldGameName="/Script/Baam", NewGameName="/Script/BG")
```
`DefaultEngine.ini:253-254` 의 낡은 `TP_Blank` 항목 2줄은 이 기회에 삭제한다.

```bash
rm -rf Intermediate/ Binaries/ DerivedDataCache/ .vs/
# 우클릭 → Generate Visual Studio project files
```

---

## Phase 4 — 프로젝트 파일 개명

```bash
git mv Baam.uproject BG.uproject
```

- 프로젝트 폴더명 `Baam/` → `BG/` — **팀원 로컬 경로가 끊기므로 공지 필요**
- Steam appid 는 프로젝트명과 무관 — 영향 없음
- 에디터 "최근 프로젝트" 목록이 끊긴다 — 정상

---

## Phase 5 — Content 재편 ✅

애셋 16개 이동. 실제 결과:

```
Content/
  Core/       BP_BGGameMode  BP_BGPlayerController  BP_BGCharacter
    Input/    IA_Look  IMC_Player
  Data/       DT_BGCard  DT_BGCharacterRow
    Character/ AT_Sheriff
  UI/
    Game/     WBP_BGHUD  WBP_BGCard  WBP_BGHand  WBP_BGPlayZone
              WBP_BGSeatBoard  WBP_BGSeat  WBP_BGTurnPanel
    Session/  WBP_BGSessionMenu
  GameSystem/ TestMap  (World Partition — 손대지 않음)
  PolygonFantasyKingdom/  (마켓 애셋 — 손대지 않음)
```

### ⚠️ 순서가 반대여야 했다 — 이 단계의 핵심 발견

처음에 애셋부터 옮기려 하자 **정확히 3건이 실패**했다.

| 애셋 | 하드코딩 위치 |
|---|---|
| `DT_BaamCard` | `DefaultGame.ini` 의 `CardTable=` |
| `DT_BaamCharacterRow` | `BGDataSubsystem.cpp` + ini |
| `WBP_BaamSessionMenu` | `BGSessionMenuController.h` 의 CDO 기본값 |

`RenameAssets` 는 **고칠 수 없는 참조자**가 있으면 거부한다. 컴파일된 C++ 은
다시 쓸 수 없기 때문이다. 나머지 13개는 애셋끼리만 참조하므로 통과했다.

→ **코드/Config 를 새 경로로 먼저 바꾸고, 리빌드한 뒤, 애셋을 옮긴다.**
잠깐 코드가 존재하지 않는 애셋을 가리키는 상태가 되지만 그건 무해하다
(로드 실패일 뿐이다). 이 순서로 16/16 성공했다.

### 자동화 방법

에디터의 `AssetTools` 를 거쳐야 하므로 `PythonScriptPlugin` 을 **임시로** 켜고
헤드리스로 실행했다. 끝난 뒤 플러그인은 다시 제거했다.

```
UnrealEditor-Cmd.exe BG.uproject -run=pythonscript -script=... -unattended -nosplash -nop4
```

> `EditorAssetLibrary.rename_asset` 은 실패해도 **예외 없이 `False` 만 반환한다.**
> 반환값을 반드시 확인할 것. `AssetTools.rename_assets`(복수형)는 `-unattended`
> 커맨드릿에서 아무 일도 하지 않고 조용히 끝났다 — 쓰지 말 것.

### 제외한 것과 그 이유

| 대상 | 이유 |
|---|---|
| `TestMap` | World Partition. `__ExternalActors__` 148개가 맵 경로에 묶여 있다. 헤드리스 이동에서 가장 위험한데 얻는 건 정리뿐이다. |
| `PolygonFantasyKingdom` (2,873개) | 이름만으로 이미 서드파티임이 분명하다. 리다이렉터 2,873개를 만들 이유가 없다. |

둘 다 나중에 에디터에서 눈으로 보며 옮기는 편이 낫다.

### 검증 (커맨드릿으로 자동 확인)

| 항목 | 결과 |
|---|---|
| `DT_BGCard` 로드 | Row 12종 |
| **`AbilityByCardId`** | **11건 — 이주 전 바이너리와 키 목록 완전 동일** |
| 매핑된 GA | 9건 전부 `GA_BGCardEffects` |
| 위젯 클래스 | `WBP_BGHUD_C` / `WBP_BGHand_C` / `WBP_BGSessionMenu_C` 정상 |
| 리다이렉터 | 0개 (고아 1개 제거) |

`AbilityByCardId` 의 값 2건이 `None` 인데 **이주 전 애셋에서도 그랬다.**
옛 `.uasset` 바이너리를 직접 대조해 확인했다 — 키 11개와 `Card.Id.Bang` 을 포함한
태그 문자열이 전부 일치한다. 개명 사고가 아니라 원래 비어 있던 항목이다.

### 알려진 기존 문제 (이번 개편과 무관)

에디터 시작 시 `CDO Constructor (BGDataSubsystem): Failed to find /Game/Data/DT_BGCardProbability`
가 뜬다. 그 애셋은 원래부터 없었다(옛 경로에서도 같은 오류였다).
감사 문서 §2.6 의 "확률 기반 카드 분배" 병렬 시스템으로, **새 리포에서 제외 권장** 대상이다.

---

## Phase 5 절차 (참고)

애셋 이동은 **반드시 에디터 안에서** 한다 (탐색기에서 옮기면 참조가 깨진다).

```
Content/
  Core/     BP_BGGameMode           ← Test/BP_TestGamemode 에서 이동·개명
            BP_BGPlayerController   ← Test/BP_TestPlayerController
  Data/     DT_BGCard  DT_BGCharacterRow  Character/AT_Sheriff
  UI/
    Game/     WBP_BGHUD  WBP_BGHand  WBP_BGCard  WBP_BGPlayZone
              WBP_BGSeatBoard  WBP_BGSeat  WBP_BGTurnPanel
    Session/  WBP_BGSessionMenu
  Maps/     TestMap
  ThirdParty/PolygonFantasyKingdom/     ← 마켓 에셋을 우리 것과 섞지 않는다
```

**`Test/` 에서 빼내는 것이 이 단계의 핵심이다.** `AbilityByCardId` 가 들어 있는
`BP_TestPlayerController` 가 `Test/` 폴더에 있으면 언젠가 누군가 "테스트용이니 지워도
되겠지" 라고 판단한다. 이 게임에서 가장 중요한 설정이다.

이동 후 반드시 **`Content` 우클릭 → Fix Up Redirectors in Folder.**
안 하면 빈 리다이렉터가 새 리포까지 따라간다.

경로를 하드코딩한 곳도 함께 고친다.
```ini
# Config/DefaultGame.ini
[/Script/BG.BGDataSettings]
CharacterTable=/Game/Data/DT_BGCharacterRow.DT_BGCharacterRow
CardTable=/Game/Data/DT_BGCard.DT_BGCard

# Config/DefaultEngine.ini
GameDefaultMap=/Game/Maps/TestMap.TestMap
EditorStartupMap=/Game/Maps/TestMap.TestMap
```

---

## Phase 4 + 6 — 폴더 개명과 새 리포 이전 ✅

둘을 합쳤다. 새 위치로 복사하는 순간 폴더 이름이 정해지므로 개명이 따로 필요 없다.

```
C:\NBC_8_Project\8th-Team3-Final-Project\        ← 팀 리포 (NBcampUnrealTrack)
  .gitattributes   .gitignore   README.md
  BG\                                            ← 프로젝트 (한 단계 유지)
    BG.uproject  Config/  Content/  Docs/  Source/
```

**평탄화하지 않았다.** 이 리포에는 프로젝트가 두 개 들어갈 예정이다.

### 복사 기준 — 추적 파일만

`git ls-files` 가 내놓은 **282개만** 복사했다. 이렇게 하면 새 리포의 첫 커밋이
검증을 마친 커밋 상태와 정확히 일치한다. 미추적 파일이 섞여 들어가거나
반대로 누락되는 일이 구조적으로 불가능해진다.

| 검증 항목 | 결과 |
|---|---|
| git 이 인식한 신규 파일 | 282 (기대치와 일치) |
| 무시 규칙에 걸려 누락된 것 | **0건** |
| 디스크에 없는 것 | **0건** |
| 클린 빌드 | `Result: Succeeded` |

### 대상 리포에서 확인한 것

**Git LFS 가 이미 설정돼 있다** — `.gitattributes` 가 `*.uasset` `*.umap` `*.png`
`*.jpg` `*.jpeg` `*.tga` `*.exr` 를 LFS 로 보낸다. 애셋이 자동으로 LFS 를 타므로
따로 할 일이 없다. 팀원은 `git lfs install` 이 되어 있어야 한다.

대상의 `.gitignore` / `.gitattributes` / `README.md` 는 **건드리지 않았다.**
프로젝트별 규칙은 전부 `BG/.gitignore` 에 모았다 — 루트에 두면 나중에 들어올
다른 프로젝트까지 영향을 받는다.

### 마켓 애셋 (Polygon Fantasy Kingdom, 2,718 파일)

구 리포가 `.gitignore` 로 제외하고 있었다. **그 결정을 유지했다.**
다만 `TestMap` 이 이 지형·메시를 참조하므로 **디스크에는 복사**했다.
`BG/.gitignore` 에 규칙과 이유를 적어 두었다.

> ⚠️ 새로 클론하는 팀원은 이 팩을 Fab 에서 받아 `BG/Content/` 아래에 두어야
> `TestMap` 이 정상으로 열린다. 안 그러면 지형과 배경이 통째로 비어 보인다.

### 이주하지 않은 것

`Docs/SampleImage` `Docs/Tools` `Docs/UI_Reconstructed` `Docs/UI_Sample` 은
**빈 폴더였다.** 안의 파일은 이주 전에 백업 후 삭제됐다. `Docs/MDRW.exe` 는
`Docs/*.exe` 규칙으로 계속 제외된다 — 저장소에 실행 파일을 넣지 않는다.

### 구 리포는 남긴다

히스토리를 새로 시작했으므로 `github.com/Serbin2/Baam` 이 유일한 기록 보관소다.
**지우지 말 것.** 태그 `baseline-before-restructure` 부터 각 Phase 커밋까지
개편 과정이 전부 그쪽에 있다.

---

## Phase 6 절차 (참고)

```bash
# 이전 직전 — 추적 누락 확인. 이 두 줄을 반드시 실행할 것.
git status --short          # ?? 로 시작하는 줄이 남아 있으면 안 된다
git ls-files Docs/          # 문서가 전부 잡히는지 눈으로 확인
```

> 🔥 형식적 절차가 아니다. 직전 작업에서 추적되지 않던 문서 4개
> (`Card-Implementation-Workflow.md` 51KB 포함)가 커밋 전에 유실됐다.
> **이주는 미추적 파일을 조용히 버린다.**

새 리포의 `.gitignore` 는 문서를 기본 추적으로 뒤집는다.
```gitignore
# Docs 는 기본 추적. 예외만 명시적으로 무시한다.
!Docs/**
Docs/*.exe          # 저장소에 바이너리를 넣지 않는다 (히스토리에 영구히 남는다)
```

**마지막 검증은 반드시 클린 클론에서 한다.** 기존 작업 디렉터리는 `Intermediate/` 와
DDC 가 남아 있어 누락된 파일이 있어도 빌드가 통과한다.

**구 리포는 아카이브로 남긴다.** 히스토리를 버리기로 한 이상 구 리포가 유일한 기록이다.

---

## 6. 이주 후 검증 체크리스트

| # | 항목 | 통과 기준 |
|---|---|---|
| 1 | 클린 클론 빌드 | `Result: Succeeded` |
| 2 | 에디터 기동 | 로드 경고 0 |
| 3 | `BG.ExpectedDeckSize 64` + `BG_DumpDeck` | **경고 0건** |
| 4 | `AbilityByCardId` | 카드 매핑 그대로 |
| 5 | 카드 실제 사용 | 판정 로그 정상, 효과 적용됨 |
| 6 | 리슨서버 2인 PIE | 손패 `COND_OwnerOnly` 유지 |
| 7 | 세션 메뉴 | 방 생성·참가 동작 |
| 8 | `git status --short` | 출력 없음 |

3번이 가장 중요하다. 카드 데이터·태그·BP 매핑이 한꺼번에 검증된다.

---

## 7. 개명 매핑표 (실제 결과)

Phase 2 실행 후 코드에서 추출한 것이다. 손으로 적은 초안이 아니다.

### 7.1 개명된 타입 61건

| 현재 | 변경 | | 현재 | 변경 |
|---|---|---|---|---|
| `ABaamCharacter` | `ABGCharacter` | | `FOnBangEndTurnRequested` | `FOnBGEndTurnRequested` |
| `ABaamGameMode` | `ABGGameMode` | | `FOnBangSeatSelected` | `FOnBGSeatSelected` |
| `ABaamGameState` | `ABGGameState` | | `FOnBangTargetSelectionCancelled` | `FOnBGTargetSelectionCancelled` |
| `ABaamPlayerController` | `ABGPlayerController` | | `IBaamSessionInterface` | `IBGSessionInterface` |
| `ABaamPlayerState` | `ABGPlayerState` | | `UBaamAbilitySystemComponent` | `UBGAbilitySystemComponent` |
| `EBaamCardEffectOp` | `EBGCardEffectOp` | | `UBaamAttributeSet` | `UBGAttributeSet` |
| `EBaamDiceOutcome` | `EBGDiceOutcome` | | `UBaamCardViewLibrary` | `UBGCardViewLibrary` |
| `EBaamSessionPhase` | `EBGSessionPhase` | | `UBaamDataSettings` | `UBGDataSettings` |
| `FBaamCardEffect` | `FBGCardEffect` | | `UBaamDataSubsystem` | `UBGDataSubsystem` |
| `FBaamCardInstance` | `FBGCardInstance` | | `UBaamDiceComponent` | `UBGDiceComponent` |
| `FBaamCardProbabilityRow` | `FBGCardProbabilityRow` | | `UBaamGameInstance` | `UBGGameInstance` |
| `FBaamCardRow` | `FBGCardRow` | | `UBaamGameplayAbility` | `UBGGameplayAbility` |
| `FBaamCharacterRow` | `FBGCharacterRow` | | `UBaamMatchStartComponent` | `UBGMatchStartComponent` |
| `FBaamFindSessionsEvent` | `FBGFindSessionsEvent` | | `UBaamReadyComponent` | `UBGReadyComponent` |
| `FBaamLobbyStatus` | `FBGLobbyStatus` | | `UBaamSeatViewLibrary` | `UBGSeatViewLibrary` |
| `FBaamOssPolicy` | `FBGOssPolicy` | | `UBaamSessionFlow` | `UBGSessionFlow` |
| `FBaamOutcomeEffects` | `FBGOutcomeEffects` | | `UBaamSessionInterface` | `UBGSessionInterface` |
| `FBaamOutcomeMagnitudes` | `FBGOutcomeMagnitudes` | | `UBaamSessionMenuController` | `UBGSessionMenuController` |
| `FBaamOutcomeWeights` | `FBGOutcomeWeights` | | `UBaamSessionMenuWidget` | `UBGSessionMenuWidget` |
| `FBaamPendingStatus` | `FBGPendingStatus` | | `UBaamSessionRowHandler` | `UBGSessionRowHandler` |
| `FBaamSessionBoolEvent` | `FBGSessionBoolEvent` | | `UBangCardDragDropOperation` | `UBGCardDragDropOperation` |
| `FBaamSessionSearchFilter` | `FBGSessionSearchFilter` | | `UBangCardWidget` | `UBGCardWidget` |
| `FBaamSessionSearchResult` | `FBGSessionSearchResult` | | `UBangHandWidget` | `UBGHandWidget` |
| `FBangCardView` | `FBGCardView` | | `UBangPlayZoneWidget` | `UBGPlayZoneWidget` |
| `FBangSeatView` | `FBGSeatView` | | `UBangSeatBoardWidget` | `UBGSeatBoardWidget` |
| `FBangTargetRequest` | `FBGTargetRequest` | | `UBangSeatWidget` | `UBGSeatWidget` |
| `FBangTurnView` | `FBGTurnView` | | `UBangTurnPanelWidget` | `UBGTurnPanelWidget` |
| `FOnBaamHandChanged` | `FOnBGHandChanged` | | `UGA_BaamCardEffects` | `UGA_BGCardEffects` |
| `FOnBaamSessionListReady` | `FOnBGSessionListReady` | | `UGE_Damage` | `UGE_BGDamage` |
| `FOnBaamSessionPhaseChanged` | `FOnBGSessionPhaseChanged` | | `UGE_Heal` | `UGE_BGHeal` |
| `FOnBangCardPlayRequested` | `FOnBGCardPlayRequested` | | | |

### 7.2 이름을 유지한 타입 13건

`FPendingHostRequest`, `FPendingJoinRequest`, `UGA_AreaBang`, `UGA_Bang`, `UGA_DrawN`, `UGA_Duel`, `UGA_GeneralStore`, `UGA_Heal`, `UGA_Missed`, `UGA_Saloon`, `UGA_StealOrDiscard`, `UGE_Equip_DwarfGloves`, `UGE_Equip_WitchCharm`

`UGA_*` 9종은 게임 안의 카드 이름이고(§2.2), `UGE_Equip_*` 2종은 개별 아이템,
`FPending*Request` 2종은 애초에 프로젝트 접두사가 없던 내부 구조체다.

### 7.3 타입이 아닌 것

| 종류 | 현재 | 변경 | 단계 |
|---|---|---|---|
| 태그 네임스페이스 | `Bang::` | `BG::` | 2 ✅ |
| 디버그 네임스페이스 | `BaamDebug::` | `BGDebug::` | 2 ✅ |
| 로그 카테고리 | `LogBaamCard` `LogBaamNet` `LogBangUI` `LogBangSeatUI` | `LogBG*` | 2 ✅ |
| 콘솔 변수 | `Bang.Debug` `Bang.ExpectedDeckSize` `Bang.IgnoreTurnOrder` | `BG.*` | 2 ✅ |
| exec 명령 | `Baam_DumpDeck` `Baam_PrintDeck` `Baam_DealHand` `Baam_DumpHand`<br>`Baam_EndTurn` `Baam_DumpTurn` `Baam_Damage` `Baam_Host` `Baam_Join` … | `BG_*` | 2 ✅ |
| 로컬라이제이션 | `NSLOCTEXT("Bang", …)` | `NSLOCTEXT("BG", …)` | 2 ✅ |
| 에디터 카테고리 | `Category = "Baam\|Turn"` `"Bang\|Card"` | `"BG\|…"` | 2 ✅ |
| 세션 설정 키 | `"BaamGameName"` `"BaamRoomCode"` `"BaamRoomName"` | `"BG*"` | 2 ✅ |
| 모듈 API | `BAAM_API` | `BG_API` | 3 |
| 모듈 경로 | `/Script/Baam.` | `/Script/BG.` | 3 |
| 애셋 이름 | `DT_BaamCard` `WBP_BaamSessionMenu` `WBP_Bang*` | `DT_BGCard` … | 5 |

> ⚠️ **세션 설정 키를 바꾸면 구버전 빌드와 방 목록이 서로 보이지 않는다.**
> Phase 3 의 모듈 개명으로 어차피 호환이 끊기므로 함께 처리했다.
> 팀원 전원이 같은 시점에 갱신해야 한다.

---

## 8. 함께 처리할 것 (감사 문서 §2 의 `[이주]` 항목)

파일을 어차피 전부 만지므로 같이 처리하면 추가 비용이 거의 없다.
**폐기가 아니라 경로 차단이다** (GDD §13.1) — 코드는 남기고 호출 경로만 끊는다.

| 항목 | 처리 |
|---|---|
| §2.2 참조 0건 DT 필드 (`Magnitude`, `ModifierTags`) | `[미구현]` 주석 유지, DT 제거 검토 |
| §2.3 `PerformDrawCheck` 껍데기 | `[미구현]` 표기 유지 |
| §2.4 구형 주사위 경로 | 새 리포에서 제외 검토 |
| §2.5 행운 보정 체인 | 위와 동일 |
| §2.6 확률 기반 카드 분배 (`FBGCardProbabilityRow`) | 병렬 시스템 통째 — **제외 권장** |
| §2.7 비활성 GA 9종 | ✅ Phase 1 에서 `GAS/Ability/Legacy/` 로 격리 완료 |
| §4.4 유령 타입 주석 | Phase 2 에서 자동 해소 (`FBangCard` 등이 아예 사라짐) |

---

## 9. 롤백

| Phase | 방법 |
|---|---|
| 1 | `git revert` — 애셋 무관, 안전 |
| 2 | `git revert` 후 애셋 13개 리세이브. 리다이렉트를 **역방향**으로 넣으면 더 안전 |
| 3 | `git reset --hard` + `Intermediate/` 삭제 + 프로젝트 파일 재생성 |
| 전체 | `git reset --hard baseline-before-restructure` |

Phase 3 이후 되돌리기가 어려워지므로 **Phase 2 통과 상태를 태그로 남긴다.**
```bash
git tag restructure-phase2-ok
```

---

## 부록 — 이 문서가 다루지 않는 것

- 게임플레이 로직 변경 (개편은 **동작을 바꾸지 않는다**)
- 감사 문서 Part 1~3 의 `[지금]` 항목 (완료됨)
- 모듈 분할 (`BGCore` / `BGUI` 등) — 11,830줄 단일 모듈에는 아직 이르다.
  다만 Phase 1 에서 Public/Private 를 진짜로 강제해 뒀으므로 그때 비용이 크게 준다.
- 유실된 `Card-Implementation-Workflow.md` 의 재작성

# 코드 감사 및 리포지토리 이주 계획

작성 기준일: 2026-07-31 / 대상 커밋: `d2ebd32` / 근거: `Game-Design-Document.md` v0.5

이 문서는 두 가지를 담는다.

1. **지금 고쳐야 하는 것** — 이주와 무관하게 동작·진단에 영향을 준다.
2. **이주할 때 같이 정리할 것** — 지금 하면 비싸고, 새 리포에서 하면 싸다.

각 항목에 **[지금]** / **[이주]** 표시를 달아 두었다. 이주 시점에만 싼 작업(클래스 이름 변경 등)을
지금 하려 들면 BP 애셋이 깨지므로 구분이 중요하다.

---

> **진행 상황 (2026-07-31)** — Part 7 의 "이주 전" 항목 2–7 완료, 컴파일 확인.
> 항목 1(미추적 문서 커밋)은 **파일이 먼저 유실되어 불가능해졌다** → §5.1.
> 남은 수동 작업은 에디터의 `DT_BaamCard` 재임포트 하나뿐이다.

## 0. 요약

| 구분 | 건수 | 성격 |
|---|--:|---|
| 🔴 진단 역전 | 3 | 가드가 반대로 작동 — 최우선 |
| 🟡 죽은 코드 | 6군 | 살아있는 진입점을 가짐 |
| 🟡 비활성 GA 표기 누락 | 7 | 실수로 되살아날 위험 |
| 🟢 단순 중복 | 4 | 저위험 |
| 🔵 구조 | 8 | 이주 시 처리 |
| ⚫ 리포 위생 | 3 | **이주 전에 반드시** |

코드는 컴파일되고 게임도 동작한다. 문제는 **"조용한 실패를 잡으려고 만든 장치가 지금은
정상 카드를 오탐하고, 진짜 문제는 놓친다"** 는 점이다.

---

# Part 1 — 🔴 진단이 거짓말을 한다 **[지금]**

뱅 피해 0 사건(§13.2 참고)을 재발시키지 않으려고 만든 가드 두 개가 현재 반대로 작동한다.

## 1.1 `Baam_DumpDeck` 이 정상 카드 9종을 "미구현" 으로 신고

**위치** `Source/Baam/Private/Game/BaamCardExec.cpp:106`

```cpp
const bool bUnimplemented = (Row != nullptr) && !Row->AbilityEventTag.IsValid() && !Row->EquipEffect;
```

**사실관계** `Docs/Data/DT_BaamCard.json` 의 **12행 전부 `AbilityEventTag` 가 비어 있다.**
카드 라우팅이 `AbilityByCardId`(카드 태그 → GA) + `OutcomeEffects` 로 옮겨간 뒤, `AbilityEventTag`
는 페이로드 로그용으로만 남았다. 검사만 옛 기준에 머물러 있다.

**증상** 덱을 덤프할 때마다 다음 경고가 뜬다. 전부 정상 동작하는 카드다.

```
GA/GE 미지정 카드 9종이 덱에 포함되어 있습니다 (사용해도 효과 없음).
```

**수정** ✅ 완료. `AbilityEventTag` 를 판정 기준에서 제외하고, 검사를 **카드 타입별로 분리**했다.

```cpp
const bool bIsEquip = (Row != nullptr) && Row->TypeTag == Bang::Card::Type::Blue.GetTag();

//	장비: 걸어줄 GE 가 있는가
const bool bNoEquipEffect = (Row != nullptr) && bIsEquip && !Row->EquipEffect;
//	갈색: 효과 목록이 있는가 (1.2 와 같은 검사)
const bool bNoEffectData  = (Row != nullptr) && !bIsEquip && Row->OutcomeEffects.IsEmpty();
```

> **초안에서 바뀐 점**: 처음 제안한 `bUnimplemented = bIsEquip ? !EquipEffect : OutcomeEffects.IsEmpty()`
> 는 1.2 의 `bNoEffectData` 와 갈색 카드에서 조건이 겹쳐 같은 카드가 두 번 신고된다.
> 카드 타입으로 갈라 두 검사가 배타적이 되게 했다. `NumUnimplemented` 카운터는
> `NumNoEquipEffect` 로 이름을 바꿨다.

## 1.2 진짜 빈 카드를 잡는 가드가 꺼져 있다

**위치** `Source/Baam/Private/Game/BaamCardExec.cpp:117`

```cpp
const bool bNoEffectData = (Row != nullptr) && !bIsEquip
    && Row->OutcomeEffects.IsEmpty() && Row->OutcomeMagnitudes.IsAllZero();
```

**사실관계** `Card-Data-Spec.md` 는 *"`OutcomeMagnitudes` 는 비활성이다. 전부 0 으로 비워 둔다"*
고 적어 두었지만, DT 는 **12행 중 10행이 `Success:1 / CriticalSuccess:2` 를 그대로 갖고 있다.**
(0 인 것은 뱅·맥주 두 행뿐이다.)

**증상** `&& IsAllZero()` 가 항상 false 이므로, 새 카드에서 `OutcomeEffects` 를 빠뜨려도
에러가 뜨지 않는다. **이 검사가 존재하는 유일한 이유가 무력화된 상태다.**

**수정** ✅ 완료. 비활성 필드를 조건에서 뺐다. 두 스키마를 함께 보는 것 자체가 원래 문제였다.

```cpp
const bool bNoEffectData = (Row != nullptr) && !bIsEquip && Row->OutcomeEffects.IsEmpty();
```

**병행** ✅ `Docs/Data/DT_BaamCard.json` 의 `OutcomeMagnitudes` 12행을 전부 0 으로 되돌렸다
(20줄 변경, `Success:1→0` / `CriticalSuccess:2→0` 뿐).
**⚠️ 에디터에서 `DT_BaamCard` 재임포트가 남았다** — 하지 않으면 애셋은 옛 값 그대로다.

## 1.3 판정 로그의 `수치` 가 실제와 다르다

**위치** `Source/Baam/Private/Player/BaamPlayerController.cpp:751, 754, 766`

```cpp
const int32 Magnitude = CardRow->OutcomeMagnitudes.ForOutcome(Outcome);
Payload.EventMagnitude = static_cast<float>(Magnitude);
...
TEXT("[판정] %s → %s  (눈 %d / 확률 ...)  수치 %d")
```

**사실관계** `UGA_BaamCardEffects` 는 `EventMagnitude` 를 **읽지 않는다.** 효과는 전부
`OutcomeEffects` 에서 온다. 따라서 이 값은 계산·전송되지만 아무 데도 쓰이지 않고, 로그에만 뜬다.

| 카드 | 로그 표시 | 실제 |
|---|---|---|
| 뱅 성공 | `수치 0` | 피해 1 |
| 함정 성공 | `수치 1` | 피해 없음(상태 부여) |

**수정 방향** 판정 로그에서 `수치` 를 뺀다. 바로 다음 줄에서 GA 가 실제 효과 요약
(`피해 1`, `1장 뽑기 + 사용한도 1 회복` 등)을 찍으므로 정보 손실이 없다.
`Payload.EventMagnitude` 대입은 아래 2.1 과 함께 처리한다.

---

# Part 2 — 🟡 살아있는 진입점을 가진 죽은 코드

"선언만 남기고 경로를 끊는다"(§13.1)는 방침에는 맞지만, **끊겼다고 적어놓고 실제로는 매 프레임/
매 카드마다 계산되는** 것들이다. 이주 시 옮길지 말지 결정이 필요하다.

## 2.1 `FBaamOutcomeMagnitudes` → `EventMagnitude` 체인 **[지금]**

카드를 낼 때마다 계산되어 페이로드에 실린다. 읽는 쪽은 비활성 GA 3종뿐이다.

| 읽는 곳 | 상태 |
|---|---|
| `GA_Bang.cpp:73` | 비활성 — `AbilityByCardId` 에 매핑 안 됨 |
| `GA_Heal.cpp:57` | 비활성 |
| `GA_StealOrDiscard.cpp:50` | 비활성 |

`BaamGameDataTypes.h:88` 의 주석 *"전용 GA 폴백 경로(EventMagnitude)가 아직 이 값을 읽는다"*
는 **더 이상 사실이 아니다.** 주석부터 고쳐야 한다.

## 2.2 참조 0건인 DT 필드 **[이주]**

| 필드 | 위치 | C++ 참조 |
|---|---|--:|
| `CheckChance` | `BaamGameDataTypes.h:311` | 0 |
| `Magnitude` | `BaamGameDataTypes.h:302` | 0 |
| `ModifierTags` | `BaamGameDataTypes.h:327` | 0 |

`ModifierTags` 는 GDD §4.3 "보정 허용 태그" 를 위해 미리 만든 자리다. 기획이 살아 있으므로
남기되, **주석에 "아직 아무도 읽지 않는다" 를 명시**해야 한다. 지금은 구현된 것처럼 보인다.

## 2.3 `PerformDrawCheck` — 껍데기 **[이주]**

**위치** `Source/Baam/Private/Game/BaamGameState.cpp:169`

```cpp
bool ABaamGameState::PerformDrawCheck(int32 Seat, float SuccessChance)
{
	//	TODO : 나중에 구현
	return false;
}
```

인자 둘 다 무시하고 상수를 반환한다. public 선언(`BaamGameState.h:42`)이라 BP/외부에서
호출 가능하고, 호출하면 **조용히 항상 실패**한다. 감옥·다이너마이트가 들어올 때 필요하지만,
그때까지는 선언을 `protected` 로 내리거나 주석에 `[미구현]` 을 다는 편이 안전하다.

## 2.4 구형 주사위 경로 — 판정 시스템이 두 벌 **[이주]**

`UBaamDiceComponent` 안에 완결된 판정 시스템이 **둘** 들어 있다.

| | 구형 | 신형(정식) |
|---|---|---|
| 진입점 | `RollForOutcome` | `RollOutcome` |
| 비율 출처 | 컴포넌트 고정값 4종 | 카드 DT `OutcomeWeights` |
| 방식 | 24면체 굴려 구간 분류 | 가중 추첨 |
| 살아있는 호출처 | **없음**(`GA_Missed:46` — 비활성) | `BaamPlayerController:697` |

구형에 딸린 것: `RollDice` / `ClassifyRoll` / `RollForOutcome` / `DiceFaces` /
`CriticalFailureRatio` / `FailureRatio` / `SuccessRatio` / `CriticalSuccessRatio`.

**문제는 양이 아니라 혼동이다.** 컴포넌트 헤더를 처음 보는 사람은 `DiceFaces = 24` 와
`SuccessRatio = 0.4` 를 보고 이게 판정 설정값이라고 생각한다. 실제 확률은 전부 DT 에 있다.

**수정 방향** 이주 시 구형을 `Deprecated` 섹션으로 몰거나, 별도 파일
(`BaamDiceComponent_Legacy.h`)로 분리한다.

## 2.5 행운 보정 체인 **[이주]**

`ApplyLuck`(`BaamDiceComponent.cpp:140`) → 호출처 없음 → `FBaamOutcomeWeights::ApplyBonus`
(`BaamGameDataTypes.h:54`) 도 연쇄적으로 미사용. `LuckSuccessWeightPerPoint` /
`LuckCriticalWeightPerPoint` 도 같다.

의도적 비활성이고 주석도 잘 달려 있다(§13.2). 다만 `ApplyBonus` 와 `ApplyBonusAll` 이
이름만으로는 구분되지 않아, 새로 보는 사람이 `ApplyBonus` 를 쓸 위험이 있다.

## 2.6 확률 기반 카드 분배 — 병렬 시스템 통째로 **[이주]**

**위치** `Source/Baam/Private/Game/BaamGameMode.cpp:173-243`

| 구성 요소 | 줄 수 |
|---|--:|
| `DrawWeightedCard` | 43 |
| `DealCards` | 27 |
| `FBaamCardProbabilityRow` | 20 |
| `GetAllCardProbabilities` | 20 |
| `UBaamDataSettings::CardProbabilityTable` | — |

호출처 없음. 주석에 "[현재 미사용]" 이 명시돼 있고 이유(복원추출이라 `InstanceId` 가 없어
덱 기반과 섞이면 안 된다)도 정확하다. **이주 시 통째로 들고 가지 않는 것을 권한다** —
덱 기반 분배가 확정된 이상 되살릴 시나리오가 없다.

## 2.7 비활성 GA 9종 — 표기가 들쭉날쭉 **[지금, 저비용]**

총 **609줄**. §13.1 방침대로 남기는 건 맞다. 문제는 **표기 일관성**이다.

| 파일 | 줄 수 | `[비활성]` 표기 |
|---|--:|:-:|
| `GA_Bang.cpp` | 148 | ✅ |
| `GA_StealOrDiscard.cpp` | 144 | ❌ |
| `GA_Heal.cpp` | 92 | ✅ |
| `GA_Missed.cpp` | 64 | ❌ |
| `GA_AreaBang.cpp` | 35 | ❌ |
| `GA_DrawN.cpp` | 33 | ❌ |
| `GA_Duel.cpp` | 31 | ❌ |
| `GA_GeneralStore.cpp` | 31 | ❌ |
| `GA_Saloon.cpp` | 31 | ❌ |

표기가 없는 7개는 **그냥 미완성 스텁으로 보인다.** 누군가 `AbilityByCardId` 에 매핑하면
반쯤 구현된 경로를 타고, `GA_DrawN`/`GA_Duel`/`GA_Saloon` 같은 건 TODO 만 있어
카드가 소비되고 아무 일도 일어나지 않는다(1.1·1.2 의 가드도 이걸 못 잡는다 — 그건 DT 검사다).

**수정 방향** 7개 파일 머리에 한 줄씩 추가한다. 5분 작업이고 사고를 막는다.

```cpp
// [비활성] AbilityByCardId 에 매핑하지 말 것 — 본문이 TODO 스텁이다.
//   매핑하면 카드가 소비되고 아무 효과도 나지 않는다. 정식 경로는 GA_BaamCardEffects + OutcomeEffects.
```

---

# Part 3 — 🟢 단순 중복 **[지금]**

## 3.1 `SetPhaseTag` vs `SetPhase` — 같은 일, 두 이름

| | `SetPhaseTag` (`BaamGameState.cpp:56`) | `SetPhase` (`:309`) |
|---|---|---|
| 권한 검사 | ✅ `HasAuthority()` | ❌ 없음 |
| 같은 값 무시 | ✅ | ❌ |
| 로그 | `[GameState] 페이즈 →` | `[Turn] 좌석 %d — %s` |
| 호출처 | `BeginPlay` 1곳 | 턴 진행 전체 |

턴 코드가 `SetPhase` 만 쓰므로 **모든 페이즈 전환이 권한 검사를 우회한다.**
현재 호출처가 전부 서버 경로라 실버그는 아니지만, 클라에서 부를 수 있는 자리에
`SetPhase` 를 한 번 넣으면 조용히 갈라진다.

**수정 방향** `SetPhase` 를 없애고 `SetPhaseTag` 로 합치되, 로그 문구는 좌석을 포함한
`SetPhase` 쪽을 살린다.

## 3.2 나머지

| 항목 | 위치 |
|---|---|
| `#include "Game/BaamGameplayTags.h"` 2회 | `BaamGameState.cpp:12-13` |
| `GetDeck` 의 불필요한 `OutDeck.Empty()` (다음 줄에서 대입) | `BaamGameState.cpp:670` |
| 빈 생성자(주석만) | `BaamPlayerController.cpp:26-30` |
| 능력치 4종이 여전히 복제됨 — 읽는 곳 0 | `BaamAttributeSet.cpp:56-59` |

능력치 복제는 값이 캐릭터 생성 시 한 번만 세팅되므로 실제 대역폭 비용은 거의 없다.
§13.1 대로 선언을 남기는 이상 `GetLifetimeReplicatedProps` 등록도 함께 남기는 편이
"되살릴 때 한 군데만 고치면 된다" 는 점에서 낫다. **조치 불필요, 인지만.**

---

# Part 4 — 🔵 구조 재편 **[이주]**

## 4.1 `PublicIncludePaths` 가 Public/Private 분리를 무력화한다

**위치** `Source/Baam/Baam.Build.cs`

```csharp
PublicIncludePaths.Add(ModuleDirectory);
```

이 한 줄 때문에 `Private/` 안의 헤더도 전부 include 가능하다. 즉 **현재 Public/Private 구분은
문서적 관습일 뿐 강제되지 않는다.** `Source/Baam/UI/` 가 Public/Private 없이 평평한 채로
동작하는 것도 이 덕분이다.

이주 시 둘 중 하나를 택한다.

| 선택 | 장점 | 비용 |
|---|---|---|
| **A. 분리를 진짜로 강제** — 이 줄 제거 | 모듈 경계가 컴파일러로 보장됨. 나중에 모듈을 쪼갤 때 필수 | 헤더를 Public/Private 로 재배치. include 경로 수정 다수 |
| **B. 분리를 포기** — 도메인 폴더만 유지 | 지금 상태를 명시적으로 인정. 작업 비용 0 | 모듈 분할 시 다시 해야 함 |

프로토타입 단계이고 모듈이 하나(11,830줄)뿐이므로 **B 를 권한다.** 대신 Build.cs 에
"의도적으로 평평하게 간다" 는 주석을 남겨야 한다. 지금은 실수처럼 보인다.

## 4.2 `Source/Baam/UI/` 만 규칙이 다르다

다른 폴더는 `Public/` + `Private/` 로 갈라져 있는데 UI 만 `.h`/`.cpp` 가 한곳에 섞여 있다.
4.1 에서 B 를 택하면 **나머지를 UI 에 맞춰 평평하게** 만드는 것이 일관적이다.

### 제안 트리 (선택지 B 기준)

```
Source/Baam/
  Baam.Build.cs
  Baam.h / Baam.cpp
  Core/        GameMode, GameState, PlayerState, PlayerController, GameInstance, Character
  Cards/       CardType, GameDataTypes, DataSubsystem, DataSettings, DiceComponent
  Abilities/   BaamGameplayAbility, GA_BaamCardEffects, GE_*, AttributeSet, ASC
  Abilities/Legacy/   GA_Bang, GA_Heal, GA_Missed, GA_DrawN, GA_Duel, ...  (비활성 9종)
  Session/     BaamSessionFlow, BaamOssPolicy, BaamSessionTypes, ...
  UI/          BangCardWidget, BangHandWidget, BangSeatWidget, ... + View 구조체
  Debug/       BaamDebug, BaamCardLog, BaamNetLog, BaamCardExec, BaamSessionExec
```

**핵심은 `Abilities/Legacy/` 다.** 비활성 GA 9종을 물리적으로 분리하면 §2.7 의 표기 문제가
폴더 구조로 해결된다 — 주석을 안 읽어도 위치만으로 알 수 있다.

## 4.3 `Bang` 과 `Baam` 접두사가 섞여 있다 **← 이주 시점에만 싸다**

| 접두사 | 개수 | 대상 |
|---|--:|---|
| `Bang*` | 14 | UI 위젯 전부 + 뷰모델 구조체 |
| `Baam*` | 30+ | 게임플레이 전반 |

`Bang` 은 원작 보드게임에서 온 초기 이름이다. 프로젝트명이 BAAM 으로 확정된 이상 정리 대상이지만,
**지금 바꾸면 BP 애셋이 깨진다** — `WBP_BangCardWidget` 등 7개 위젯이 이 C++ 클래스를 상속한다.

### 이주 시 처리 방법

1. 새 리포에서 클래스명을 `UBaamCardWidget` 등으로 변경
2. `Config/DefaultEngine.ini` 에 리다이렉트를 넣어 기존 BP 를 살린다

```ini
[CoreRedirects]
+ClassRedirects=(OldName="/Script/Baam.BangCardWidget", NewName="/Script/Baam.BaamCardWidget")
+ClassRedirects=(OldName="/Script/Baam.BangHandWidget", NewName="/Script/Baam.BaamHandWidget")
; ... 14개
+StructRedirects=(OldName="/Script/Baam.BangCardView", NewName="/Script/Baam.BaamCardView")
```

3. 에디터에서 BP 를 열어 저장(리다이렉트 적용) → 리다이렉트 제거

**함께 볼 것**: 게임플레이 태그 네임스페이스 `Bang::` (`BaamGameplayTags.h`)와
콘솔 변수 `Bang.IgnoreTurnOrder` / `Bang.ExpectedDeckSize`.
태그 **문자열**(`Card.Id.Bang` 등)은 접두사가 없으므로 영향 없다 — DT 와 BP 가 문자열로
참조하므로 **절대 건드리지 말 것.**

> 이름 통일은 기능적 이득이 없다. 새 팀원이 "Bang 은 뭐고 Baam 은 뭔가" 를 묻지 않게 하는 것이
> 유일한 목적이다. 이주 외의 시점에는 비용 대비 효과가 없다.

## 4.4 유령 타입 — 존재하지 않는 것을 참조하는 주석

| 주석에 등장 | 실제 |
|---|---|
| `FBangCard` / `UBangCardDef` (`UI/BangCardView.h:11`) | 선언 없음 |
| `UBaamCardDef` (`BaamGameplayTags.h:20`) | 선언 없음 |
| `FBangResolutionRequest` (`GA_Missed.cpp:52`, `BaamGameplayTags.h:137`) | 선언 없음 |

"게임 로직 쪽 카드 타입(`FBangCard` / `UBangCardDef`)에 의존하지 않는다" 같은 문장은
**존재하지 않는 타입과의 독립성을 설명하고 있다.** 실제 로직 타입은 `FBaamCardInstance` 다.
읽는 사람이 없는 파일을 찾게 만든다.

## 4.5 Content 폴더 — 이름이 사실과 다르다

```
Content/GameSystem/Test/BP_TestPlayerController   ← AbilityByCardId 가 여기 있다
Content/GameSystem/Test/BP_TestGamemode
```

**`AbilityByCardId` 는 이 게임에서 가장 중요한 설정이다.** 이게 비면 모든 카드가 죽는다
(`Card-Data-Spec.md` 도 "이걸 채우지 않으면 카드가 아무 일도 하지 않는다" 고 경고한다).
그런 설정이 `Test/` 폴더에 있으면 누군가 "테스트용이니 지워도 되겠지" 라고 판단할 수 있다.

또한 UI 루트가 둘로 갈려 있다.

```
Content/UI/            WBP_Bang*  (게임 플레이 UI 7종)
Content/Network/UI/    WBP_BaamSessionMenu
```

### 제안

```
Content/
  Core/       BP_BaamGameMode, BP_BaamPlayerController   ← Test/ 에서 이동·개명
  Data/       DT_BaamCard, DT_BaamCharacterRow, Characters/AT_Sheriff
  UI/
    Game/     WBP_BaamCardWidget, WBP_BaamHand, WBP_BaamHUD, WBP_BaamSeatBoard, ...
    Session/  WBP_BaamSessionMenu
  Maps/       TestMap
```

BP 애셋 이동은 에디터 안에서 드래그하면 리다이렉터가 자동 생성된다. **이동 후 반드시
`Fix Up Redirectors in Folder` 를 실행**하고 커밋해야 한다 — 안 그러면 새 리포에
빈 리다이렉터가 따라간다.

---

# Part 5 — ⚫ 리포 위생 **[이주 전에 반드시]**

## 5.1 🔥 추적되지 않던 문서 — **유실됨**

`git ls-files Docs/` 결과와 실제 파일을 비교했을 때 다음이 git 밖에 있었다.

| 파일 | 크기 | 결과 |
|---|--:|---|
| `Docs/Card-Implementation-Workflow.md` | 51 KB | 🔥 **유실** |
| `Docs/UI-CardPlay-Setup.md` | 6.6 KB | 🔥 **유실** |
| `Docs/Tools/Build-GameDesignDocument.ps1` | — | 🔥 **유실** |
| `Docs/Tools/Test-GameDesignDocument.ps1` | — | 🔥 **유실** |

**2026-07-31 정리 작업 도중 네 파일이 디스크에서 사라졌다.** 커밋하기 전이었다.

- git 에 없으므로 `git checkout` / `git fsck` 로 복구할 수 없다
- 휴지통에도 없다(하드 삭제)
- `C:\Users\njh10` 아래 어디에도 남아 있지 않다

**복구를 시도한다면**: 에디터 로컬 히스토리(VS Code `File > Local History`, Rider
`Local History`), Windows 파일 히스토리 / 이전 버전, OneDrive 휴지통 순으로 확인한다.
`Card-Implementation-Workflow.md` 는 STEP 1–8 체크리스트 913줄이라 손실이 가장 크다.

### 교훈 — 이주 전 필수 절차

**미추적 파일은 이주 시점이 아니라 "만든 날" 커밋한다.** 이번처럼 정리 작업 중에
사라지면 되돌릴 방법이 없다. 새 리포에서는 다음을 초기 커밋에 넣을 것.

```gitignore
# Docs 는 기본 추적. 예외만 명시적으로 무시한다.
!Docs/**
Docs/*.exe
```

이주 직전에 반드시 확인한다.

```
git status --short          # ?? 로 시작하는 줄이 남아 있으면 안 된다
git ls-files Docs/          # 실제 파일 목록과 대조
```

## 5.2 `Docs/MDRW.exe` — 저장소 안의 바이너리

163 KB 실행 파일이 `Docs/` 에 있다(미추적). 마크다운 도구로 보이는데,

- 커밋하면 리포에 바이너리가 영구히 남는다(git 은 삭제해도 히스토리에 남음)
- 미추적으로 두면 다른 작업자에게 전달되지 않는다

**권장**: `.gitignore` 에 `Docs/*.exe` 를 추가하고, 도구가 필요하면 `Docs/Tools/README.md` 에
**어디서 받는지**를 적는다. 새 리포에 바이너리를 들고 가지 않는다.

## 5.3 `.gitignore` 는 양호

UE 표준 항목이 잘 갖춰져 있다(`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`,
`Content/Developers/`, `*_BuiltData.uasset`). 이주 시 그대로 쓰면 된다. 추가 권장은
5.2 의 `Docs/*.exe` 한 줄뿐이다.

---

# Part 6 — 확인만 해둘 것

## 6.1 `ConsumeCard` 가 GA 실행 뒤에 온다

**위치** `BaamPlayerController.cpp:800`(GA 발동) → `:823`(카드 소비)

GA 가 도는 동안 **낸 카드가 아직 손패에 있다.** 발동 실패 시 카드를 되돌려야 하므로
의도된 순서이고, 지금은 자기 손패를 버리는 카드가 없어 문제없다.

다만 나중에 "자기 손패 무작위 버리기" 를 추가하면 **낸 카드가 자기 자신을 버릴 수 있다.**
`EBaamCardEffectOp` 에 그런 Op 를 추가할 때 이 순서를 먼저 확인해야 한다.
지금은 코드 옆에 주석 한 줄이면 충분하다.

## 6.2 문서와 DT 의 드리프트

`Card-Data-Spec.md` 는 `OutcomeMagnitudes` 전부 0 을 명시하지만 DT 는 10행이 비어 있지 않다
(§1.2). 문서가 옳고 DT 가 뒤처졌다. 재임포트로 해소된다.

---

# Part 7 — 실행 순서

## 이주 전 (현재 리포) — **완료 (2026-07-31)**

| # | 작업 | 근거 | 상태 |
|--:|---|---|---|
| 1 | 미추적 문서 커밋 | §5.1 | 🔥 **유실** — §5.1 참고 |
| 2 | `Docs/*.exe` gitignore | §5.2 | ✅ |
| 3 | 진단 가드 3건 수정 | §1.1–1.3 | ✅ |
| 4 | DT `OutcomeMagnitudes` 0 으로 | §1.2 | ✅ JSON / ⚠️ 재임포트 남음 |
| 5 | 비활성 GA 7개에 표기 추가 | §2.7 | ✅ |
| 6 | `SetPhase`/`SetPhaseTag` 통합 + 중복 include | §3.1–3.2 | ✅ |
| 7 | 유령 타입 주석 정정 | §4.4 | ✅ |

함께 처리한 것: `GetDeck` 의 불필요한 `Empty()`, 빈 생성자 제거(헤더 선언 포함),
`PerformDrawCheck` 에 `[미구현]` 경고, `Magnitude`/`ModifierTags` 에 `[미사용]`/`[미구현]` 표기.

컴파일 확인: `Build.bat BaamEditor Win64 Development` → `Result: Succeeded`.

### 남은 수동 작업

**에디터에서 `DT_BaamCard` 재임포트** — 이걸 하지 않으면 §1.2 의 절반만 적용된 상태다
(코드는 고쳐졌지만 애셋에는 옛 magnitude 가 남아 있다). 기능에는 영향이 없지만,
DT 를 다시 내보낼 때 옛 값이 되살아난다.

## 이주 시 (새 리포)

| # | 작업 | 근거 |
|--:|---|---|
| 8 | Source 폴더 재편 + `Abilities/Legacy/` 분리 | §4.2 |
| 9 | Build.cs `PublicIncludePaths` 결정 및 주석 | §4.1 |
| 10 | `Bang*` → `Baam*` 개명 + CoreRedirects | §4.3 |
| 11 | Content 재편 (`Test/` → `Core/`, UI 통합) | §4.5 |
| 12 | 확률 분배 시스템 제외하고 이주 | §2.6 |
| 13 | 구형 주사위 경로 분리 또는 제외 | §2.4 |

**10번은 8·11번과 같은 커밋에 묶지 말 것.** 클래스 개명은 BP 재저장이 필요하고, 폴더 이동과
섞이면 무엇이 깨졌는지 분리해 볼 수 없다.

## 이주 후 검증

```
Bang.ExpectedDeckSize 64
Baam_DumpDeck      → 11 종류 / 64 장, 경고 0 건
Baam_PrintDeck     → 셔플 순서 출력
Baam_DumpTurn      → 좌석·페이즈·사용 한도
```

`Baam_DumpDeck` 이 **경고 없이** 끝나는 것이 §1.1·§1.2 수정의 합격 기준이다.
그 뒤 `Card-Data-Spec.md` 의 테스트 시나리오 14종을 돌린다.

---

## 부록 — 이 문서가 다루지 않는 것

- **GDD §14 미결 항목** — 자동 방어 판정 순서, 보정 합산 방식 등은 기획 결정이라 제외
- **미구현 기능** — 확률 표시 UI(§10), 이벤트 로그 패널, 요정 갑옷 방어 판정(G)
- **성능** — 좌석 보드 0.25 초 폴링(`BaamPlayerController.cpp:203` TODO)은 프로토타입
  범위에서 문제없다고 판단

# 뱅(BANG!) 언리얼 프로토타입 작업 플로우

**목표**: 그래픽은 최소, 규칙과 온라인은 실제로 동작 — "친구 4~7명이 방코드로 모여 한 판을 끝까지 플레이하고 재미를 판단할 수 있는 빌드"를 최단 경로로 만든다.

**대상 엔진**: UE 5.8 / 모듈명 `Baam` / C++ 주도, 블루프린트는 UI와 데이터 애셋에만

---

## 0. 전제와 범위

### 가정 (다르면 알려주세요)
- 기본판(Base game)만. 확장(Dodge City, High Noon 등)은 비범위.
- 4~7인. 역할 분배: 4인 = 보안관/배신자/무법자×2, 5인 = +부관1, 6인 = +무법자1, 7인 = +부관1.
- 봇(AI)은 **테스트 가속용으로만** 1종(랜덤 합법수) 구현. 재미 검증은 사람으로 한다.
- 아트/사운드/애니메이션 없음. 카드 = 텍스트가 적힌 버튼. 플레이어 = 좌석 위젯.

### 명시적 비범위
재접속·호스트 마이그레이션, 관전, 랭킹/계정, 로컬라이즈, 치팅 방지, 확장 카드, 3D 카드 연출.

---

## 1. 아키텍처 결정 (먼저 못 박아야 할 것)

### 1.1 GAS의 경계 — 가장 중요한 결정

GAS는 **카드 효과 실행과 스탯 변형**에는 잘 맞지만, **턴 진행과 응답 대기(우선권)**에는 네이티브 기능이 없습니다. 룰 엔진을 GAS 안에 넣으면 "Missed!를 기다리는 중" 같은 상태를 어빌리티 안에서 표현하다가 빠르게 무너집니다.

| 계층 | 담당 | 구현 |
|---|---|---|
| **규칙 / 우선권** | 턴 페이즈, 응답 창, 순서, 승패 판정 | GameState의 서버 권위 상태 머신 (**GAS 아님**) |
| **효과 실행** | 카드 1장이 하는 일 | `UGameplayAbility` 1개 = 1개 *메커니즘* |
| **지속 상태** | 무기 사거리, Mustang/Scope, 감옥, 다이너마이트 | `UGameplayEffect` (Infinite) + `GameplayTag` |
| **스탯** | 생명력, 사거리, 거리 보정, 턴당 BANG 한도 | `UAttributeSet` |
| **카드/덱 데이터** | 80장 덱, 손패, 버린 패 | 평범한 서버 권위 배열 + DataTable (GAS 무관) |

> 어빌리티는 **카드별이 아니라 메커니즘별**로 만듭니다. `GA_Bang`, `GA_Heal`, `GA_DrawN`, `GA_StealOrDiscard`, `GA_AreaBang`, `GA_Duel`, `GA_Saloon`, `GA_GeneralStore` — 8~10개면 갈색 카드 대부분이 커버됩니다. 개별 카드는 DataAsset이 어빌리티 + 파라미터를 지정합니다.

### 1.2 턴제이므로 예측(Prediction)을 쓰지 않는다

GAS 복잡도의 절반은 클라이언트 예측입니다. 턴제 카드 게임은 예측이 필요 없습니다.

- 어빌리티 `NetExecutionPolicy = **ServerOnly**`
- GAS 인풋 바인딩을 쓰지 않는다. 클라이언트는 `ServerRequestPlayCard(CardInstanceId, TargetSeat)` 커스텀 RPC를 PlayerController에 보내고, **서버가 검증 후** `ASC->TryActivateAbility()` 호출.
- `InstancingPolicy = InstancedPerActor`

### 1.3 은닉 정보 (카드 게임의 핵심 제약)

`PlayerState`에 손패를 그냥 replicate하면 **모두에게 보입니다.**

```cpp
// ABangPlayerState
UPROPERTY(Replicated) TArray<FBangCard> Hand;       // COND_OwnerOnly ← 필수
UPROPERTY(Replicated) int32            HandCount;   // 전원 공개
UPROPERTY(Replicated) TArray<FBangCard> Equipment;  // 전원 공개 (파란 카드는 공개 정보)
UPROPERTY(Replicated) EBangRole        Role;        // 보안관만 공개, 나머지는 사망 시 공개

void ABangPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME_CONDITION(ABangPlayerState, Hand, COND_OwnerOnly);
    DOREPLIFETIME(ABangPlayerState, HandCount);
    DOREPLIFETIME(ABangPlayerState, Equipment);
}
```

덱과 버린 패 원본은 **GameState에 서버 전용(non-replicated)** 으로 두고, `DeckCount` / `DiscardTop` / `DiscardCount`만 복제합니다.

> ⚠️ **리슨서버 한계**: 호스트는 서버 프로세스를 갖고 있어 메모리에서 모든 손패를 읽을 수 있습니다. 프로토타입에서는 수용하고, 필요해지면 데디케이티드 서버로 옮기는 게 유일한 근본 해결입니다. 지금 대응하지 마세요.

### 1.4 맵 이동을 하지 않는다

로비 → 게임을 `ServerTravel`로 처리하면 PlayerState 재생성, 심리스 트래블 설정, ASC 재초기화 버그를 한 번에 다 만나게 됩니다.

**단일 맵에서 GameState의 페이즈 태그만 `Phase.Lobby` → `Phase.Play`로 전환**하세요. 트래블 관련 문제군 전체를 회피합니다.

### 1.5 좌석 기반 거리 — 월드 좌표를 쓰지 않는다

뱅의 거리는 **살아있는 플레이어들로 이루어진 원**에서의 최단 걸음 수입니다. 사망자는 원에서 빠집니다. 액터 위치로 계산하면 반드시 틀립니다.

```cpp
// 살아있는 좌석만 모아 원형 거리 계산 후 Scope/Mustang 보정
int32 UBangSeating::GetDistance(const ABangGameState* GS, int32 FromSeat, int32 ToSeat)
{
    const TArray<int32> Alive = GS->GetAliveSeatsInTableOrder();
    const int32 N = Alive.Num();
    const int32 i = Alive.IndexOfByKey(FromSeat);
    const int32 j = Alive.IndexOfByKey(ToSeat);
    if (i == INDEX_NONE || j == INDEX_NONE) return TNumericLimits<int32>::Max();

    const int32 Step = FMath::Abs(i - j);
    const int32 Raw  = FMath::Min(Step, N - Step);

    // Scope: 내가 남을 볼 때 -1  /  Mustang: 남이 나를 볼 때 +1
    const int32 Mod = Raw
        - GS->GetSeatAttribute(FromSeat, DistanceReduction)
        + GS->GetSeatAttribute(ToSeat,   DistanceIncrease);

    return FMath::Max(1, Mod);   // 거리는 1 미만으로 내려가지 않는다
}
```

---

## 2. 클래스 골격

```
ABangGameMode        서버 전용. 좌석 배정, 역할/캐릭터 분배, 덱 생성, 승패 판정
ABangGameState       페이즈 상태 머신, 좌석 목록, 응답 큐, 덱/버린패(서버전용) + 카운트
ABangPlayerState     ASC + AttributeSet 소유. 손패/장비/역할/캐릭터/생존
ABangPlayerController  클라 입력 RPC, 응답 프롬프트 수신
UBangAbilitySystemComponent
UBangAttributeSet
UBangCardDef         (UPrimaryDataAsset) 카드 1종 정의
UBangCharacterDef    (UPrimaryDataAsset) 캐릭터 1종 정의
UBangResolutionSubsystem  (또는 GameState 내부) 응답 창 처리
UAbilityTask_WaitForResolution  응답을 기다리는 어빌리티 태스크
```

**폰은 만들지 않습니다.** ASC의 `InitAbilityActorInfo(PlayerState, PlayerState)` — OwnerActor와 AvatarActor 모두 PlayerState. 카메라는 레벨에 배치한 `ACameraActor` 하나를 `SetViewTarget`으로 고정.

### AttributeSet

| Attribute | 기본 | 용도 |
|---|---|---|
| `Health` / `MaxHealth` | 캐릭터별 3~4 (보안관 +1) | 탄환. 손패 한도의 기준도 됨 |
| `WeaponRange` | 1 (Colt .45) | Schofield 2 / Remington 3 / Rev.Carabine 4 / Winchester 5 |
| `DistanceReduction` | 0 | Scope, Rose Doolan → +1 |
| `DistanceIncrease` | 0 | Mustang, Paul Regret → +1 |
| `BangLimit` | 1 | Volcanic, Willy the Kid → 큰 수 |
| `MissedRequired` | 1 | Slab the Killer → 2 |
| `DrawCount` | 2 | 턴 시작 뽑기 장수 |
| `DrawBangCount` | 1 | Lucky Duke의 "뽑기 2장 중 선택" |

> 손패 한도 = `Health` (별도 어트리뷰트 불필요). 예외 캐릭터가 생기면 `HandLimitBonus` 추가.

### GameplayTag 분류

```
Card.Type.{Brown, Blue}
Card.Id.{Bang, Missed, Beer, Saloon, Stagecoach, WellsFargo, CatBalou, Panic,
         Duel, GeneralStore, Indians, Gatling, Jail, Dynamite, Barrel,
         Mustang, Scope, Volcanic, Schofield, Remington, RevCarabine, Winchester}
Card.Trait.{Weapon, NeedsTargetSeat, Reaction, RangeLimited}
State.{Dead, Jailed, HasDynamite}
State.Turn.{Active, BangPlayed}
Phase.{Lobby, TurnStart, Draw, Play, Discard, GameOver}
Response.Allow.{Missed, Bang, PickRevealed, PickFromPlayer}
Character.{Ability.LuckyDuke, Ability.CalamityJanet, ...}
```

---

## 3. 응답 창(Resolution) 설계 — 이 프로젝트의 진짜 난이도

BANG!에 Missed!로 반응하기, 결투에서 BANG!을 번갈아 내기, 인디언 습격에 순서대로 반응하기, 잡화점에서 순서대로 고르기 — 전부 같은 문제입니다: **서버가 특정 플레이어에게 물어보고 답을 기다린다.**

### 핵심 단순화: 활성 요청은 항상 1개

여러 명이 동시에 응답하는 UX는 나중 최적화입니다. **좌석 순서대로 한 명씩** 처리하면 추론과 디버깅이 훨씬 쉽고, 실제 보드게임 진행과도 같습니다.

```cpp
UENUM()
enum class EBangResponseKind : uint8
{
    PlayCardFromHand,     // Missed! / Bang!(결투)
    PickFromRevealed,     // 잡화점
    ChooseTargetSeat,     // 대상 지정
    PickFromPlayer,       // Panic! / Cat Balou (손패 뒷면 or 장비 선택)
    DiscardToHandLimit,   // 턴 종료 버리기
};

USTRUCT()
struct FBangResolutionRequest
{
    int32 RequestId = 0;
    EBangResponseKind Kind = EBangResponseKind::PlayCardFromHand;
    FGameplayTag SourceEffect;            // Card.Id.Bang 등
    int32 SourceSeat = INDEX_NONE;
    int32 RespondingSeat = INDEX_NONE;    // 지금 답할 사람 (항상 1명)
    FGameplayTagContainer AllowedCards;   // Response.Allow.*
    TArray<FBangCard> Choices;            // PickFromRevealed용
    int32 RequiredCount = 1;              // Slab the Killer = Missed! 2장
    double DeadlineServerTime = 0.0;
};
```

### 흐름 (BANG! 카드 예시)

1. 클라 → `ServerRequestPlayCard(BangCardId, TargetSeat)`
2. 서버 검증: 내 턴인가 / `Phase.Play`인가 / `WeaponRange >= GetDistance()`인가 / `State.Turn.BangPlayed` 태그 + `BangLimit` 한도
3. 서버가 `ASC->TryActivateAbility(GA_Bang)`
4. `GA_Bang`이 `PushRequest{ Bang, RespondingSeat=Target, Allow=[Missed] }` 후 `UAbilityTask_WaitForResolution`으로 대기
5. 서버 → `ClientPromptResponse(Request)` (해당 좌석 PC에만)
6. 클라 → `ServerSubmitResponse(RequestId, Payload | Pass)`
7. 서버 검증 후 태스크 브로드캐스트 → `GA_Bang`이 이어서 진행 (Missed! → 무효 / Pass → `GE_Damage` 1)

### 반드시 넣어야 하는 것: 데드라인 워치독

`GameState`가 서버에서 `DeadlineServerTime` 경과를 감시해 **자동 Pass로 강제 해소**합니다. 없으면 한 명이 응답 UI를 무시하거나 접속을 끊는 순간 게임이 영구히 멈춥니다. 프로토타입에서 가장 자주 만나는 정지 원인입니다.

이 워치독은 **접속 종료로 PlayerState가 파괴되어 진행 중 어빌리티가 중단되는 경우**도 함께 덮어줍니다.

### 카드별 매핑

| 카드 | 요청 구성 |
|---|---|
| BANG! | 대상 1명, `Allow=[Missed]`, `RequiredCount = 대상의 MissedRequired` |
| 개틀링 / 인디언 습격 | 공격자 기준 시계방향 살아있는 전원에게 요청을 **순차로 push** (개틀링 `Allow=[Missed]`, 인디언 `Allow=[Bang]`) |
| 결투 | 두 좌석을 번갈아 `Allow=[Bang]`. Pass한 쪽이 피해 1 |
| 잡화점 | 살아있는 인원 수만큼 공개 → 현재 플레이어부터 시계방향 `PickFromRevealed` 순차 |
| Panic! / Cat Balou | `PickFromPlayer` — 대상의 손패(뒷면 인덱스) 또는 장비 중 선택 |
| 배럴 | **뽑기(Draw!)**. 프로토타입에서는 Missed! 프롬프트에 "배럴 사용" 버튼을 함께 노출하는 옵트인 방식으로 단순화 |
| 감옥 / 다이너마이트 | 응답 창이 아니라 **턴 시작 페이즈의 뽑기 판정**. 상태 머신 쪽 |

---

## 4. 카드 구현 우선순위 — 재미 검증 최단 경로

전체 80장을 다 만들고 나서 테스트하면 안 됩니다. **T1만 끝나면 이미 플레이 가능**합니다.

### T1 — 게임이 성립하는 최소 집합
`BANG!` · `Missed!` · `Beer` · 기본 무기(Colt, 사거리 1) · 역할 4종 · 턴 진행 · 좌석 거리 · 승패 판정
캐릭터는 **특수능력 없는 더미 1종**(HP 4)만. 
→ 이 시점에 "쏘고, 막고, 죽고, 진영이 이긴다"는 코어 루프를 검증할 수 있습니다.

### T2 — 전략성
`역마차` `웰스파고`(카드 수급) · `당황(Panic!)` `캣 발루(Cat Balou)`(견제) · 무기 5종(Volcanic / Schofield / Remington / Rev.Carabine / Winchester) · `Mustang` `Scope` `Barrel`(거리·방어, **뽑기 메커닉 도입 지점**)

### T3 — 판을 흔드는 카드
`개틀링` `인디언 습격` `결투` `술집` `잡화점` · `감옥` `다이너마이트`(턴 시작 뽑기 판정)

### T4 — 캐릭터 특수능력 16종
각각이 **규칙의 예외**이므로 코어가 안정된 뒤에 붙입니다. GAS의 진짜 값어치가 나오는 구간입니다.

| 캐릭터 | 능력 | GAS 구현 훅 |
|---|---|---|
| Paul Regret | 거리 +1 | 패시브 `GE`: `DistanceIncrease +1` |
| Rose Doolan | 거리 -1 | 패시브 `GE`: `DistanceReduction +1` |
| Willy the Kid | BANG! 무제한 | 패시브 `GE`: `BangLimit` 큰 값 |
| Slab the Killer | Missed! 2장 필요 | 패시브 `GE`: `MissedRequired 2` |
| Lucky Duke | 뽑기 2장 중 선택 | 패시브 `GE`: `DrawBangCount 2` + 뽑기 루틴 분기 |
| Jourdonnais | 배럴 내장 | 캐릭터 태그 → 배럴 판정에 조건 추가 |
| Calamity Janet | BANG!/Missed! 상호 사용 | 응답 검증에서 태그 치환 허용 |
| Bart Cassidy | 피해 1당 1장 뽑기 | `Health` 변화 감지 → `GameplayEvent` |
| El Gringo | 피해 준 자의 손패 1장 획득 | 피해 GE의 Instigator에서 트리거 |
| Suzy Lafayette | 손패 0장이면 1장 뽑기 | 손패 변경 델리게이트 |
| Sid Ketchum | 손패 2장 버려 회복 | 능동 어빌리티 (턴 중 언제든) |
| Vulture Sam | 사망자 카드 전부 획득 | 사망 이벤트 리스너 |
| Jesse Jones | 첫 장을 남의 손에서 | 뽑기 페이즈 오버라이드 |
| Pedro Ramirez | 첫 장을 버린 패에서 | 뽑기 페이즈 오버라이드 |
| Kit Carlson | 3장 보고 2장 선택 | 뽑기 페이즈 오버라이드 + `PickFromRevealed` |
| Black Jack | 2번째 공개, 하트/다이아면 1장 더 | 뽑기 페이즈 오버라이드 |

> **HP와 정확한 문구는 카드 실물에서 전사하세요.** 위 표는 구현 훅 매핑용입니다.

---

## 5. 페이즈별 작업 플로우

기간은 **1인 풀타임 실작업일** 기준의 감각치입니다. GAS 경험이 없다면 M3에 배수를 두세요.

### M0 — 프로젝트 골격 (0.5일)

1. **플러그인 활성화** (`Baam.uproject`): `GameplayAbilities`, `CommonUI`(선택 — `DefaultGame.ini`에 설정이 이미 있음), `GameplayDebugger`
2. **`Baam.Build.cs`**: `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, `UMG`, `Slate`, `SlateCore` 추가
3. **`InitGlobalData()` 호출** — 없으면 TargetData 직렬화에서 크래시합니다. GAS 최다 함정:
   ```cpp
   void UBangGameInstance::Init()
   {
       Super::Init();
       UAbilitySystemGlobals::Get().InitGlobalData();
   }
   ```
4. **렌더링 설정 다운그레이드 — 반복 속도에 직결.** 현재 `DefaultEngine.ini`는 Substrate / RayTracing / PathTracing / Lumen HWRT / Nanite가 전부 켜져 있습니다. 카드 게임에 아무 값도 없으면서 셰이더 컴파일과 에디터 기동을 크게 늘립니다. **Content가 비어 있는 지금이 끄기 가장 싼 시점입니다.**
   ```ini
   r.Substrate=False
   r.RayTracing=False
   r.PathTracing=False
   r.Lumen.HardwareRayTracing=False
   r.DynamicGlobalIlluminationMethod=0   ; None
   r.ReflectionMethod=2                  ; SSR
   r.Nanite.ProjectEnabled=False
   ```
5. **기본 맵 교체**: `GameDefaultMap`이 `/Engine/Maps/Templates/OpenWorld`입니다. 빈 레벨 `/Game/Maps/L_Table`을 만들고 `GameDefaultMap` / `ServerDefaultMap` 모두 지정. 라이트 하나 + 카메라 하나.
6. 위 클래스 골격을 컴파일만 되는 빈 껍데기로 전부 생성.

**완료 기준**: PIE 4클라이언트(Net Mode = *Play As Listen Server*)로 접속해 각 클라가 `GameState`의 좌석 배열을 로그로 출력.

---

### M1 — 카드 데이터와 덱 (1~1.5일)

1. `UBangCardDef` (`UPrimaryDataAsset`) 또는 DataTable Row:
   ```
   CardId(FName) · DisplayName · Suit(♠♥♦♣) · Rank(A,2..K)
   TypeTag(Brown/Blue) · TraitTags · GrantedAbility(TSubclassOf<UGameplayAbility>)
   EquipEffect(TSubclassOf<UGameplayEffect>) · AbilityParams(int32 등)
   ```
2. **80장 덱 테이블 전사.** 수트와 랭크가 **뽑기(Draw!) 판정에 실제로 쓰입니다** (배럴=하트, 다이너마이트=스페이드 2~9, 감옥=하트). 대략 BANG! 25장 / Missed! 12장 / Beer 6장 규모지만 **정확한 수트·랭크는 룰북 또는 카드 실물에서 전사해야 합니다.** 여기서 대충 넣으면 T2 뽑기 메커닉 전체가 틀어집니다.
3. `FBangCard { int32 InstanceId; FName CardId; }` — 인스턴스 ID로 "이 카드를 버려라"를 모호함 없이 지정.
4. GameState에 덱/버린 패(서버 전용) + 셔플(시드 고정 옵션 — **버그 재현에 필수**), 소진 시 버린 패 리셔플.
5. §1.3의 손패 복제 규칙 적용.

**완료 기준**: 서버가 각 클라에 5장씩 배분. **자기 손패만** 이름이 보이고 남은 장수만 보이는 것을 4클라 PIE에서 눈으로 확인.

---

### M2 — 턴 상태 머신 + 응답 창 (2~3일) ★ 최대 리스크 구간

1. 페이즈 진행: `TurnStart`(감옥/다이너마이트 판정) → `Draw`(`DrawCount`장) → `Play` → `Discard`(손패 한도 = `Health`) → 다음 살아있는 좌석
2. §3의 `FBangResolutionRequest` + 큐 + `ClientPromptResponse` / `ServerSubmitResponse`
3. **데드라인 워치독** (§3) — 미루지 마세요. 이게 없으면 이후 모든 테스트가 정지로 끝납니다.
4. `UAbilityTask_WaitForResolution`
5. 뽑기(Draw!) 공용 함수: 덱 맨 위 공개 → 수트/랭크 판정 → 버린 패로. `DrawBangCount`가 2면 2장 공개 후 선택 요청.

**완료 기준**: 카드 효과 하나도 없이 4인이 턴을 돌리고, 더미 요청("계속하려면 아무 키")에 응답하고, 응답 없이 방치하면 타임아웃으로 진행된다.

---

### M3 — GAS 카드 어빌리티 (2~3일)

1. `UBangAttributeSet` (§2) + `PostGameplayEffectExecute`에서 `Health` 클램프 및 사망 판정
2. **ASC 설정**:
   - `SetIsReplicated(true)`
   - `SetReplicationMode(EGameplayEffectReplicationMode::**Full**)` — 7명 턴제라 대역폭이 무의미하고 모든 클라가 동일한 GE 상태를 보게 되어 디버깅이 훨씬 쉽습니다. `Mixed`는 나중 최적화.
   - `InitAbilityActorInfo(this, this)` — 서버는 `PostInitializeComponents`, 클라는 `OnRep_PlayerState` 경로에서 **양쪽 다** 호출
3. **T1 어빌리티**: `GA_Bang`, `GA_Missed`(응답 전용), `GA_Heal`(Beer)
4. **T2/T3 어빌리티**: `GA_DrawN`(역마차/웰스파고), `GA_StealOrDiscard`(Panic/Cat Balou), `GA_AreaBang`(개틀링/인디언), `GA_Duel`, `GA_Saloon`, `GA_GeneralStore`
5. **파란 카드** = Infinite `GE` + 공개 `Equipment` 배열. 무기는 1개만 장착(교체 시 기존 것 버림), 같은 파란 카드 중복 장착 금지.
6. 감옥/다이너마이트는 `State.Jailed` / `State.HasDynamite` 태그 + M2의 턴 시작 판정

**완료 기준**: T1 카드로 실제 사살이 발생하고 사망자가 좌석 원에서 빠지며 거리 계산이 그에 맞게 변한다.

---

### M4 — 역할·승패·죽음 처리 (1일)

- 역할 분배(§0), 보안관만 공개, 사망 시 역할 공개
- 사망 처리: 손패·장비 전부 버린 패로, 좌석 원에서 제거
- **보상/벌칙**: 무법자를 죽인 자 → 3장 뽑기 / 보안관이 부관을 죽임 → 보안관 손패·장비 전부 버림
- 승리 판정: 보안관 진영(무법자+배신자 전멸) / 무법자(보안관 사망) / 배신자(보안관을 마지막에 1:1로 처리)
- **Beer 예외**: 2인만 남았을 때 Beer는 효과가 없습니다. 놓치기 쉬운 규칙.

**완료 기준**: 한 판이 승패 화면까지 끝난다.

---

### M5 — 최소 UI (1~2일)

- 손패: 카드 이름 텍스트 버튼 목록. 3D 카드 메시 없음.
- 좌석: 원형 배치 위젯 7개 — 이름 / HP / 역할(공개 시) / 장비 목록 / 손패 장수 / **현재 거리**(디버깅에 매우 유용)
- 대상 지정: 좌석 위젯 클릭
- 응답 프롬프트: 남은 시간 카운트다운 + 사용 가능 카드 + Pass 버튼
- **로그 패널**: "A가 B에게 BANG! → B가 Missed!" 전체 히스토리. 아트보다 이게 먼저입니다. 규칙 버그 대부분을 여기서 잡습니다.
- 3D는 평면 + 좌석 위치 표시용 큐브 7개면 충분합니다.

**완료 기준**: 로그와 UI만 보고 규칙 위반을 판별할 수 있다.

---

### M6 — 방코드 온라인 (1~2일) ← **마지막에 붙인다**

M5까지 네트워크 테스트는 **PIE 멀티클라이언트 + `open <IP>`** 로 충분합니다. 방코드는 배포 편의 기능이지 재미 검증에 필요한 것이 아닙니다. 먼저 붙이면 세션 디버깅에 며칠을 씁니다.

- **친구 플레이테스트는 M5 직후**, ZeroTier/Radmin 같은 가상 LAN + `open <IP>` 로 즉시 시작하세요. M6와 병행합니다.
- 엔진에 OSSv1(`OnlineSubsystemEOS`, `OnlineSubsystemSteam`)과 OSSv2(`OnlineServicesEOS`)가 모두 있습니다. **자료와 예제가 압도적으로 많은 OSSv1 + EOS**를 권합니다(Steam은 클라이언트 실행이 필요하고 appid 480 공유 이슈가 있음). OSSv2 전환은 프로토타입 이후.
- 방코드: 혼동되는 문자(0/O/1/I)를 뺀 6자. 호스트가 세션 생성 시 어트리뷰트로 광고, 참가자는 필터 검색:
  ```cpp
  // Host
  Settings.Set(FName("ROOMCODE"), Code, EOnlineDataAdvertisementType::ViaOnlineService);
  // Join
  Search->QuerySettings.Set(FName("ROOMCODE"), Code, EOnlineComparisonOp::Equals);
  ```
  생성 전 같은 코드로 검색해 충돌만 회피.
- §1.4대로 **맵 이동 없음.** 참가자는 `?listen` 호스트에 접속만 하고 `Phase.Lobby`에서 대기.

**완료 기준**: 친구가 코드 6자만 받아 인터넷을 통해 접속해 한 판을 끝낸다.

---

## 6. 재미 검증 체크포인트

| 시점 | 인원 | 확인할 질문 |
|---|---|---|
| M2 완료 | 4 (PIE) | 턴이 답답하지 않은가? 응답 타임아웃 몇 초가 적당한가? |
| **M3 + T1** | 4 (PIE/LAN) | **"쏘고 막는" 순간이 재미있는가?** 여기서 재미없다면 카드를 더 넣어도 해결되지 않습니다 |
| M4 완료 | 4~5 | 역할 추리가 작동하는가? 한 판 길이가 적당한가? |
| M5 + T2 | 5~6 | 정보(로그/UI)만으로 판단할 수 있는가? 카드 수급이 답답하지 않은가? |
| M6 + T3 | 6~7 | 판을 뒤집는 순간이 나오는가? |
| +T4 | 7 | 캐릭터 특능이 차이를 만드는가? |

**M3+T1 플레이테스트가 가장 중요합니다.** 대략 6~8일차에 도달 가능하고, 여기서 나오는 피드백이 T2 이후의 우선순위를 바꿉니다.

---

## 7. 함정 목록

### 뱅 규칙
- 거리는 **살아있는** 플레이어의 원 기준. 사망 시 모든 거리가 변합니다.
- Scope는 *내가 볼 때* 감소, Mustang은 *남이 나를 볼 때* 증가. 방향을 섞으면 조용히 틀립니다.
- 거리는 1 미만이 되지 않습니다.
- 손패 한도 = **현재** 생명력 (최대치가 아님).
- 2인만 남으면 Beer 무효.
- 다이너마이트는 판정에 실패하면 다음 플레이어로 **넘어갑니다** (제거가 아님).
- 무기는 1개만. 교체 시 기존 무기는 버린 패로.
- 보안관은 MaxHealth +1.

### UE / GAS
- `InitGlobalData()` 누락 → TargetData 크래시. 1순위 확인 사항.
- `InitAbilityActorInfo`를 서버/클라 **양쪽에서** 호출하지 않으면 클라 UI가 어트리뷰트를 못 읽습니다.
- 어트리뷰트를 세터로 직접 바꾸지 말고 **서버에서 GE로** 변경하세요. 안 그러면 예측/복제가 어긋납니다.
- `COND_OwnerOnly` 누락 = 전원에게 손패 노출. **첫 4클라 PIE 테스트에서 반드시 눈으로 확인하세요.**
- 응답 대기 중 접속 종료 → PlayerState 파괴 → 어빌리티 중단 → 진행 정지. 워치독이 유일한 방어선입니다.
- Iris 리플리케이션을 켜지 마세요. 카드 게임에 이득이 없고 디버깅 표면만 늘어납니다.
- 셔플 시드를 로그에 남기세요. 재현 없는 카드 게임 버그는 추적이 매우 어렵습니다.

---

## 8. 일일 반복 루프

1. 아침: 어제의 규칙 버그를 로그 패널 히스토리로 재현 (시드 고정)
2. 카드 1~2종 또는 응답 창 1종을 추가
3. PIE 4클라이언트로 즉시 확인 (Net Mode = Play As Listen Server)
4. 커밋 단위 = **카드 1종 또는 메커니즘 1종**
5. 주 1회: 사람 4인 이상 실제 플레이테스트

---

## 9. 요약 타임라인

```
M0 골격         0.5일  ├ 누적 0.5
M1 카드/덱      1.5일  ├ 누적 2
M2 턴/응답창    3일    ├ 누적 5     ★ 최대 리스크
M3 GAS + T1     2일    ├ 누적 7     ★ 첫 재미 검증
M4 역할/승패    1일    ├ 누적 8     한 판 완주
M5 최소 UI      2일    ├ 누적 10    친구 플레이테스트 시작(가상 LAN)
M6 방코드       2일    ├ 누적 12    코드로 접속 가능
T2/T3 카드      3일    ├ 누적 15
T4 캐릭터 16종  3일    └ 누적 18    기본판 기능 완성
```

**약 7일차에 첫 재미 검증, 10일차에 친구와 플레이, 12일차에 방코드 완성.**

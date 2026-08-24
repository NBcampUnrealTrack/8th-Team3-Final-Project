# 코드 리뷰 지적사항 추적

작성 기준일: 2026-08-19 / 대상 브랜치: `BG` / 대상 커밋: `aefcaea..db4cd57`
범위: 직업 12종 어트리뷰트·훅, `NotifyDamaged`(반사·강탈), 1회성 카드, `Card.Trait.Attack` 경로, FX 컴포넌트

이 문서는 **살아 있는 목록**이다. 한 번 읽고 버리는 리뷰 결과가 아니라,
각 항목이 아직 살아 있는지 **기계적으로 다시 확인할 수 있게** 만들어 두었다.

- 고쳤으면 상태를 `[해결]` 로 바꾸고 **커밋 해시를 적는다.** 항목을 지우지 않는다
  (GDD §13.1 폐기 정책과 같은 이유 — 왜 그렇게 됐는지가 남아야 한다).
- 각 항목의 **재확인** 절은 그대로 붙여 넣으면 도는 명령이다. 아래 §재확인 스크립트에 전부 모아 두었다.
- 새 리뷰를 돌리면 여기에 `R-9` 부터 이어 붙인다.

`Code-Audit-And-Migration-Plan.md` 는 **구 리포에서 이주할 때의 정리 목록**이라 성격이 다르다.
그쪽은 이주와 함께 닫히는 문서이고, 이쪽은 계속 쌓인다.

---

## 0. 요약

| # | 상태 | 등급 | 제목 | 성격 |
|---|---|---|---|---|
| R-1 | **[해결]** | 🔴 | 1회성 카드 `InstanceId` 가 `INDEX_NONE` | 원격 클라 접속 종료 |
| R-2 | **[해결]** | 🔴 | 도둑이 "지금 내는 중인 카드" 를 훔친다 | 카드 복제·덱 회계 붕괴 |
| R-3 | **[해결]** | 🟡 | 1회성 카드가 사용한도 게이트에 막힌다 | 기능이 거의 안 보임 |
| R-4 | **[해결]** | 🟡 | `GE_Passive_Monk` 이 DT 에 연결돼 있지 않다 | 수도사 반사 전체 무효 |
| R-5 | **[해결]** | 🟡 | `NotifyDamaged` 에 자기 사망 가드가 없다 | 죽은 뒤에도 반사·강탈 |
| R-6 | **[보류 — 플레이 테스트 후 결정]** | 🟢 | 반사 가드가 강탈 경로를 덮지 않는다 | 의도인지 불명 |
| R-7 | **[오탐]** | — | ~~`HealBonus` 가 회복량 0 인 카드에도 붙는다~~ | 상위 호출부에 가드가 있다 |
| R-8 | **[해결]** | 🟢 | `DT_BGCard.json` 에 `Card.Trait.Attack` 이 없다 | 재임포트 시 조용한 파괴 |
| R-9 | **[해결]** | 🟡 | `ApplyJobVisual` 이 메쉬와 ABP 를 한 덩어리로 판단한다 | 직업 ABP 가 무시된다 |

**공통점**: 8건 중 6건이 **에러도 경고도 남기지 않았다.** R-1 은 호스트에서 재현되지 않고,
R-4·R-8 은 코드가 아니라 데이터가 비어 있는 것이라 C++ 어디를 봐도 원인이 보이지 않는다.

> **⚠️ 2026-08-19 시점에 R-1·R-2·R-3·R-5·R-8 을 고쳤고 빌드는 통과했지만, 아직 커밋되지 않았다.**
> **어느 것도 실제 플레이로 검증되지 않았다** — 아래 §플레이 검증 항목을 볼 것.

---

# R-1 🔴 1회성 카드 `InstanceId` 가 `INDEX_NONE` 이다 **[해결 — 2026-08-19]**

**조치** 시작값을 `-2` 로 내렸다(`BGGameState.h`). 음수라는 성질은 그대로다.
헤더 주석에 **"-1 로 되돌리면 안 되는 이유"** 를 남겼다 — 값만 보면 `-1` 이 자연스러워 보이므로.


**위치** `Source/BG/Public/Core/BGGameState.h:478` · `Source/BG/Private/Core/BGGameState.cpp:820`

```cpp
int32 NextEphemeralInstanceId = -1;              // BGGameState.h:478
Card.InstanceId = NextEphemeralInstanceId--;     // BGGameState.cpp:820 → 첫 카드가 -1
```

`-1` 은 `INDEX_NONE` 이다. 그리고 서버 RPC 가 그 값을 무효로 판정한다.

```cpp
// BGPlayerController.cpp:986 / :1595
bool ABGPlayerController::ServerRequestPlayCard_Validate(int32 InstanceId, int32 TargetSeat)
{ return InstanceId != INDEX_NONE; }
```

**UE 는 `_Validate` 가 false 를 반환하면 연결을 끊는다.** 즉 첫 1회성 카드를 내는 순간
그 플레이어가 **매치에서 튕긴다.** 사용(`:986`)과 버리기(`:1595`) 양쪽 다 걸린다.

같은 값이 다른 곳에서도 특별 취급된다 — 고쳐도 확인해 둘 것:

| 지점 | 증상 |
|---|---|
| `FBGCardInstance::IsValid()` · `FBGCardView::IsValid()` | 유효한 카드를 무효로 본다 |
| `UBGHandWidget::MarkCardPlayed(-1)` | 조용히 아무 일도 안 한다 |
| `FindHeldCard(-1)` (`BGHandWidget.cpp:882`) | **"아무거나 일치" 와일드카드**로 해석된다 |

**⚠️ 로컬 테스트로 절대 안 잡힌다.** 호스트는 RPC 를 타지 않아 멀쩡하다.
재현 조건이 **리슨서버 + 원격 클라이언트 + 암살자 + 첫 대성공** 이라 혼자 PIE 로 돌리면 안 보인다.

**고치는 법** — 시작값을 `-2` 로. 음수라는 성질(덱 ID 와 안 겹침)은 그대로 유지된다.

**재확인**
```bash
grep -n "NextEphemeralInstanceId = " Source/BG/Public/Core/BGGameState.h
#  = -1  이면 아직 살아 있다.  = -2  이면 해결.
```

---

# R-2 🔴 도둑이 "지금 내고 있는 카드" 를 훔친다 **[해결 — 2026-08-19]**

**조치** `ABGPlayerState::CardInFlightInstanceId`(서버 전용, 비복제) 를 추가했다.
`HandlePlayCard` 가 **`GiveAbilityAndActivateOnce` 호출 직전에 세우고 직후에 지운다** —
발동이 동기로 끝나므로 그 창 밖에서는 강탈이 일어날 수 없다. 창이 좁아 조기 반환으로 샐 여지도 없다.
강탈 루프는 후보 목록을 만들 때 이 값을 제외한다(`BGCharacter.cpp`).

**⚠️ 남의 손패에서 카드를 고르는 코드를 새로 만들면 이 값을 반드시 다시 확인해야 한다.**
`CardInFlightInstanceId` 의 헤더 주석에 같은 경고를 적어 두었다.


**위치** `Source/BG/Private/Core/BGPlayerController.cpp:1440` (소비) · `Source/BG/Private/Core/BGCharacter.cpp:453` (강탈 루프)

`UGA_BGCardEffects` 는 **완전 동기**다 — `ActivateAbility` 안에서 `ApplyDamage` 를 하고
`EndAbility`(`GA_BGCardEffects.cpp:105`) 까지 같은 호출 스택에서 끝난다. AbilityTask 가 없다.

그런데 `ConsumeCard(InstanceId)` 는 **발동이 끝난 뒤**에 온다.

```
HandlePlayCard
 └─ GiveAbilityAndActivateOnce           ← 낸 카드는 아직 가해자 손패에 있다
     └─ ApplyDamage → GE
         └─ 피해자 PostGameplayEffectExecute
             └─ NotifyDamaged → 강탈 루프가 AttackerPS->GetHand() 를 본다  ← 낸 카드가 후보에 있다
 └─ ConsumeCard(InstanceId)              ← 이미 도둑 손에 있으면 실패
```

**결과**: 버린 패로 가야 할 카드가 도둑 손패로 **복제**되고 `ConsumeCard` 는 "이미 제거됨?" 으로 실패한다.
덱 총량이 어긋나 `BG.ExpectedDeckSize 64` 검증이 깨진다.

> **이 경우는 예고돼 있었다.** `Code-Audit-And-Migration-Plan.md` §6.1 이 같은 순서를 두고
> *"지금은 자기 손패를 버리는 카드가 없어 문제없다. 다만 나중에 남의 손패를 건드리는 것이 생기면
> 이 순서를 먼저 확인해야 한다"* 고 적어 두었다. 도둑이 바로 그 변경이다.
> **§6.1 은 닫힌 항목이 아니다 — 이 항목이 그 후속이다.**

**고치는 법** (둘 중 하나)
1. 강탈 후보에서 진행 중인 `InstanceId` 를 제외한다 — 국소적이고 안전하다. **권장.**
2. `ConsumeCard` 를 발동 앞으로 옮긴다 — 발동 실패 시 카드를 되돌리는 기존 설계가 깨진다.

**재확인**
```bash
# 강탈 루프가 진행 중인 카드를 걸러내는지 본다. 아무것도 안 나오면 아직 미해결.
grep -n "InFlight\|ExcludeInstance\|PlayingInstanceId" Source/BG/Private/Core/BGCharacter.cpp
```
게임 안에서는 — 도둑으로 여러 번 맞은 뒤 `BG.ExpectedDeckSize 64` + `BG_DumpDeck`.

---

# R-3 🟡 1회성 카드가 사용한도 게이트에 막힌다 **[해결 — 2026-08-19]**

**조치** `bEphemeralCard` 판정을 **함수 맨 앞으로 올려** 게이트가 그것을 먼저 보게 했다
(`!bEphemeralCard && !HasCardUsesLeft(...)`). DT Row 조회는 그대로 아래에 두었다 —
"한도를 쓰는가" 는 손패 인스턴스만 보면 알 수 있어 무거운 조회를 앞당길 이유가 없다.

**클라이언트도 같이 고쳤다.** `UpdateHandInteractionLock` 이 한도 소진으로 손패를 통째로
잠그고 있어서, 서버가 허용해도 **낼 방법 자체가 없었다.** 손패에 1회성 카드가 있으면 잠그지 않는다.
⚠️ 잠금 단위가 손패 전체라 이건 "손패를 열어 둔다" 이지 "아무 카드나 낼 수 있다" 가 아니다 —
한도를 쓰는 카드를 내면 서버가 거부하고 사유를 띄운다. 카드 단위로 막으려면 위젯에 per-card 잠금이 필요하다.


**위치** `Source/BG/Private/Core/BGPlayerController.cpp:1032` (게이트) vs `:1102` (`bEphemeralCard` 를 세우는 곳)

```
:1032   if (... && !GS->HasCardUsesLeft(MySeat)) { 거부하고 return; }
          ⋮  70줄
:1102   bEphemeralCard = Card.bEphemeral;        ← 여기서야 1회성인지 알게 된다
```

한도를 **소모하지 않는** 카드인데 한도가 찼다는 이유로 거부된다.
기본 `CardUseLimit` 이 2 이므로 **두 번째 카드로 대성공한 암살자는 방금 받은 카드를 그 턴에 못 쓴다.**
클라이언트도 `bCanPlay` 로 손패를 통째로 비활성화하므로 눌러 볼 수조차 없다.

암살자의 능력이 "대성공하면 공짜 공격" 인데, 대성공은 보통 카드를 쓰다가 나오므로
**능력이 발동한 대부분의 경우에 쓸 수 없다.** 기능이 거의 안 보인다.

**고치는 법** — 손패 조회(→`bEphemeralCard`)를 게이트 앞으로 올리거나, 게이트에서 1회성을 예외 처리한다.

**재확인**
```bash
grep -n "HasCardUsesLeft(MySeat)\|bEphemeralCard = Card.bEphemeral" Source/BG/Private/Core/BGPlayerController.cpp
# HasCardUsesLeft 의 줄 번호가 더 작으면 아직 미해결.
```

---

# R-4 🟡 `GE_Passive_Monk` 이 DT 에 연결돼 있지 않다 **[해결 — 2026-08-19, 미커밋]**

**위치** `Content/Data/DT_BGCharacterRow.uasset` — `Character.Job.Monk` 행의 `PassiveEffects`

리뷰 시점(uasset mtime `08-18 16:45`)에는 애셋만 있고
(`Content/GAS/Effect/GE_Passive_Monk.uasset`) **DT 가 그것을 가리키지 않았다.**

```
리뷰 시점  GE_Passive_*   Assassin Brawler Diviner Healer Mage Mercenary Priest Thief   ← 8종. Monk 없음
현재       GE_Passive_*   위 8종 + Monk                                                  ← 9종. 정상
```

`DamageReflect` 가 생성자 기본값 0 에 머무르면 `BGCharacter.cpp:426` 의 반사 블록이
**한 번도 실행되지 않는다.** 재진입 가드(`GBGApplyingReflect`)도, 수도사끼리의 무한 루프 방지도
전부 도달 불가 코드가 된다. C++ 에는 아무 문제가 없어 코드를 아무리 봐도 원인이 안 보인다.

**⚠️ `DT_BGCharacterRow.uasset` 이 아직 커밋되지 않았다(`git status` 에 `M`).**
커밋하지 않으면 다른 사람의 클론에서는 여전히 반사가 죽어 있다.

> **이런 종류가 이 프로젝트에서 가장 비싸다.** 애셋을 만드는 것과 DT 에 연결하는 것이 별개의 작업이라
> "만들었으니 됐다" 로 끝나기 쉽다. `AbilityByCardId` 가 비면 카드가 죽는 것과 같은 구조다.
> **직업을 추가할 때는 애셋 생성 · DT 연결 · 커밋 세 가지가 모두 끝나야 완료다.**

**재확인** — 직업을 추가할 때마다 쓸 수 있다. 패시브가 필요한 직업 수와 맞는지 본다.
```bash
python -c "import re;d=open('Content/Data/DT_BGCharacterRow.uasset','rb').read().decode('latin-1');\
p=sorted(set(re.findall(r'GE_Passive_[A-Za-z]+',d)));print(len(p),p)"
# 9종이 나와야 정상. 8종이면 Monk 가 다시 빠진 것이다.
```

---

# R-5 🟡 `NotifyDamaged` 에 자기 사망 가드가 없다 **[해결 — 2026-08-19]**

**조치** `NotifyDamaged` 앞부분에 `if (bDead) return;` 을 넣었다.
`Magnitude` 가 클램프 전 값인 문제는 **고치지 않았다** — 아래 설명대로 지금은 무해하고,
고치려면 "실제로 깎인 양" 을 계산해야 해서 무해한 것을 위해 코드를 늘리게 된다.
**"받은 피해만큼" 동작하는 능력을 추가하는 시점에 같이 고칠 것.**


**위치** `Source/BG/Private/Core/BGCharacter.cpp:401`~`:419`

가해자는 거른다 — 자기 자신은 안 본다.

```cpp
if (!SelfPS || !AttackerPS || AttackerPS->IsDead())   // :417 — AttackerPS 만 본다
{
    return;
}
```

`NotifyHealthDepleted` 가 `bDead` 를 즉시 세우지만 **사망 규칙 처리(`HandleSeatDeath`)는 한 틱 뒤**다.
그 사이(또는 그 이후)에 들어온 피해 GE 는 여전히 `Delta < 0` 이므로
**죽은 수도사가 반사하고 죽은 도둑이 카드를 훔친다.**

**고치는 법** — `NotifyDamaged` 안에 `if (bDead) return;`.

**같이 적어 둘 것 (지금은 무해)**
`Data.EvaluatedData.Magnitude`(`BGAttributeSet.cpp:150`)는 **클램프 전 값**이다.
2HP 대상에 5 피해가 들어오면 실제로 깎인 건 2 인데 `DamageAmount` 는 5 로 전달된다.
현재 `DamageAmount` 는 `<= 0` 판별에만 쓰이므로 **당장은 아무 영향이 없다.**
"받은 피해만큼" 무언가를 하는 능력을 추가하는 순간 틀린 값으로 동작한다.

**재확인**
```bash
sed -n '401,425p' Source/BG/Private/Core/BGCharacter.cpp | grep -n "bDead"
# 아무것도 안 나오면 미해결.
```

---

# R-6 🟢 반사 가드가 강탈 경로를 덮지 않는다 **[보류 — 플레이 테스트 후 결정]**

**2026-08-19 결정 보류.** 밸런스 문제라 코드만 봐서는 답이 안 나온다. 실제로 쳐 보고 정한다.
그때까지 **코드는 현재 동작(강탈은 잠기지 않음) 그대로 둔다.**


**위치** `Source/BG/Private/Core/BGCharacter.cpp:433`

`TGuardValue<bool> ReflectGuard(GBGApplyingReflect, true)` 는 `Reflect > 0` 블록만 감싼다.
반사 피해가 가해자에게 닿으면 가해자의 `NotifyDamaged` 가 돌고, 그쪽 **강탈 블록은 잠겨 있지 않다.**

> 도둑이 수도사를 공격한다 → 수도사가 반사한다 → 그 반사 피해로 도둑의 `NotifyDamaged` 가 돌아
> **도둑이 수도사에게서 카드를 한 장 가져간다.**

무한 루프는 아니다(반사만 잠겨 있으므로 한 번에 끝난다). **의도인지 아닌지가 정해지지 않았을 뿐이다.**

- 의도라면 — 반사는 "되받아치기", 강탈은 "맞은 대가" 로 서로 다른 사건이니 일관성이 있다. 주석 한 줄만 남긴다.
- 의도가 아니라면 — 강탈 블록에도 `!GBGApplyingReflect` 를 건다.

**이 항목만 고치기 전에 판단이 필요하다.**

---

# R-7 ~~`HealBonus` 가 회복량 0 인 카드에도 붙는다~~ **[오탐 — 다시 파지 말 것]**

리뷰가 `ApplyHeal`(`GA_BGCardEffects.cpp:303`) 안에만 `Amount <= 0` 검사가 없는 것을 보고
사제가 회복량 0 인 항목에서도 `HealBonus` 만큼 회복한다고 지적했다. **틀렸다.**

가드는 **한 단계 위 디스패치 루프**에 있다.

```cpp
// GA_BGCardEffects.cpp:127
if (Effect.Amount <= 0 && Effect.Op != EBGCardEffectOp::ApplyStatus)
{
    continue;                      // ← ApplyHeal 이 애초에 호출되지 않는다
}
```

즉 `ApplyHeal` 은 `Amount > 0` 일 때만 도달한다. `ApplyDamage`(마법사 `DamageBonusChance`)도 마찬가지다.
`ApplyHeal` 안의 주석 *"회복량이 0 인 카드는 대상이 아니다"* 는 **정확한 서술**이다 —
그 규칙이 이 함수가 아니라 호출부에서 집행될 뿐이다.

**교훈**: 이 파일의 `Apply*` 함수들은 전부 `Amount > 0` 을 **전제로 한다.**
개별 함수만 보면 전부 "가드가 없다" 로 보인다. 새 `Apply*` 를 추가할 때도 이 전제를 그대로 따르면 된다.

---

# R-8 🟢 `DT_BGCard.json` 에 `Card.Trait.Attack` 이 없다 **[해결 — 2026-08-19]**

**조치** 뱅(`Card.Id.Bang`)·속공(`Card.Id.QuickStrike`) 행의 `TraitTags` 에 추가했다(+6줄).

**⚠️ 이걸로 재임포트가 안전해진 것은 아니다.** `Card.Trait.Cinematic` 은 아직 uasset 에만 있다 —
어느 카드에 붙어 있는지 바이너리로는 알 수 없어 **추측해서 넣지 않았다.**
근본 해결은 **에디터에서 uasset → JSON 을 한 번 내보내 드리프트를 없애는 것**이다.
그때까지 이 JSON 으로 재임포트하면 연출 태그가 날아간다.


**위치** `Docs/Data/DT_BGCard.json`

uasset 에는 들어 있다. **JSON 에는 0건이다** — `Card.Trait.NeedsTargetSeat` 뿐이다.

`Card-Data-Spec.md` 는 이 JSON 을 **재임포트 소스로 안내한다.** 즉 문서대로 재임포트하면
`Card.Trait.Attack` 이 통째로 날아가고 **용병과 무투가가 경고 한 줄 없이 죽는다.**
두 직업 다 이 태그에만 걸려 있다.

`Card.Trait.Cinematic` 도 같은 상태다(이전부터 알려진 드리프트).

**고치는 법** — 뱅·속공 행에 `Card.Trait.Attack` 을 JSON 에 반영. 근본적으로는
**uasset → JSON 방향의 내보내기를 한 번 해서 드리프트를 없애는 쪽**이 맞다.

**재확인**
```bash
grep -c "Card.Trait.Attack" Docs/Data/DT_BGCard.json    # 0 이면 미해결
```

---

# R-9 🟡 `ApplyJobVisual` 이 메쉬와 ABP 를 한 덩어리로 판단한다 **[해결 — 2026-08-21]**

**위치** `Source/BG/Private/Core/BGCharacter.cpp` — `ApplyJobVisual()`

`/code-review` 가 아니라 ABP 구조를 설계하다 나온 건이다. 성격이 R-4·R-8 과 같아 여기서 함께 추적한다.

직업 외형은 `FBGCharacterRow` 의 **`SkeletalMesh`(메쉬)** 와 **`AnimClass`(ABP)** 두 열로 들어온다.
그런데 코드가 둘을 하나로 묶어 판단하고 있었다. 두 갈래로 조용히 깨진다.

```cpp
if (!Row || Row->SkeletalMesh.IsNull())   { return; }   // ① 메쉬가 비면 ABP 도 건너뛴다
if (MeshComp->GetSkeletalMeshAsset() == NewMesh) { return; }   // ② 메쉬가 같으면 ABP 도 건너뛴다
```

| | 언제 터지나 | 증상 |
|---|---|---|
| ① | `SkeletalMesh` 가 비어 있을 때 | **`AnimClass` 를 채워도 아무 일도 일어나지 않는다.** 2026-08 현재 12종 메쉬가 전부 `(미정)` 이라 **지금 ABP 를 넣으면 100% 이 경우다** |
| ② | 두 직업이 같은 메쉬를 쓸 때 | 먼저 배정된 직업의 ABP 가 남는다. 임시 플레이스홀더를 공유하면 바로 걸린다 |

둘 다 **"애니메이션이 안 바뀌는데 ABP 어디를 봐도 원인이 없는"** 모양이다. 로그도 경고도 없다.

**조치** 메쉬와 ABP 를 갈라 각자 판단하게 했다. 순서는 **메쉬 먼저** — ABP 는 새 메쉬의
스켈레톤 위에서 초기화돼야 한다. 둘 다 바뀌면 초기화가 두 번 돌지만 직업 배정 때 한 번뿐이라
문제되지 않는다. 로그도 `메쉬=... / ABP=...` 로 무엇이 실제로 바뀌었는지 나누어 찍는다.

> **배경** — 2026-08-21 에 **스켈레톤을 하나로 통일**하기로 정했다(아트 기획서 §아트 구현 특이사항 유지).
> 그래서 직업 개성은 메쉬가 아니라 **ABP + 애님 세트**가 담당하고, `AnimClass` 가 실질적인 주입구가 된다.
> 이 열이 동작하지 않으면 캐릭터 12종이 전부 같은 애니메이션으로 움직인다.

**재확인**
```bash
grep -c "bMeshChanged\|bAnimChanged" Source/BG/Private/Core/BGCharacter.cpp   # 0 이면 회귀
```

---

# 재확인 스크립트

한 번에 전부 본다. 프로젝트 루트(`BG/`)에서 실행한다.

```bash
echo -n "R-1 (0이면 해결) "; grep -c "NextEphemeralInstanceId = -1" Source/BG/Public/Core/BGGameState.h
echo -n "R-2 (1이상이면 해결) "; grep -c "InFlight\|ExcludeInstance\|PlayingInstanceId" Source/BG/Private/Core/BGCharacter.cpp
echo "R-3 (bEphemeralCard 줄번호가 더 작으면 해결)"
grep -n "HasCardUsesLeft(MySeat)\|bEphemeralCard = Card.bEphemeral" Source/BG/Private/Core/BGPlayerController.cpp
echo -n "R-4 (9면 정상) "; python -c "import re;d=open('Content/Data/DT_BGCharacterRow.uasset','rb').read().decode('latin-1');print(len(set(re.findall(r'GE_Passive_[A-Za-z]+',d))))"
echo -n "R-5 (1이상이면 해결) "; sed -n '/void ABGCharacter::NotifyDamaged/,/^}/p' Source/BG/Private/Core/BGCharacter.cpp | grep -c "bDead"
echo -n "R-8 (1이상이면 해결) "; grep -c "Card.Trait.Attack" Docs/Data/DT_BGCard.json
echo -n "R-9 (1이상이면 해결) "; grep -c "bMeshChanged\|bAnimChanged" Source/BG/Private/Core/BGCharacter.cpp
```

**수정 후 실제 출력 (2026-08-19)** — 이 값과 다르면 회귀한 것이다.

```
R-1 (0이면 해결) 0
R-2 (1이상이면 해결) 2
R-3 (bEphemeralCard 줄번호가 더 작으면 해결)
128:		&& (GS->HasCardUsesLeft(MySeat) || bHasEphemeralCard);
1039:				bEphemeralCard = Card.bEphemeral;
1071:		if (GBGIgnoreTurnOrder == 0 && GS && !bEphemeralCard && !GS->HasCardUsesLeft(MySeat))
R-4 (9면 정상) 9
R-5 (1이상이면 해결) 2
R-8 (1이상이면 해결) 2
```

참고로 **수정 전**은 이랬다 — R-3 의 `1032 < 1102` 가 문제의 핵심이었다.

```
R-1 1 / R-2 0 / R-5 0 / R-8 0
R-3  1032: HasCardUsesLeft(게이트)   1102: bEphemeralCard   ← 게이트가 70줄 먼저였다
```

R-6 은 설계 결정이라, R-7 은 오탐이라 기계적으로 확인하지 않는다.

---

# 플레이 검증 — 아직 아무것도 안 했다

**위 스크립트는 "코드 모양이 바뀌었다" 까지만 증명한다.** 동작이 맞는지는 별개다.
특히 R-1 은 **정적으로도 단독 PIE 로도 절대 재현되지 않는다.**

| # | 검증 방법 | 통과 조건 |
|---|---|---|
| R-1 | **리슨서버 + 원격 클라이언트**(반드시 2인 이상). 클라를 암살자로 두고 대성공 → 받은 카드 사용 | 클라가 **튕기지 않는다** |
| R-2 | 도둑으로 여러 번 맞은 뒤 `BG.ExpectedDeckSize 64` → `BG_DumpDeck` | 경고 **0건** |
| R-3 | 한도를 다 쓴 뒤 대성공으로 받은 1회성 카드를 사용 | 손패가 잠기지 않고, 서버가 거부하지 않는다 |
| R-4 | 수도사에게 뱅 | 로그에 `[반사] 좌석 N → 좌석 M 에 피해 1` |
| R-5 | 체력 1 인 수도사·도둑을 죽이는 피해 | 그 피해에 **`[반사]`·`[강탈]` 로그가 없다** |
| R-8 | — | **DT 재임포트를 아직 하지 말 것** (`Card.Trait.Cinematic` 이 날아간다) |
| R-9 | DT 의 `AnimClass` 만 채우고(메쉬는 비운 채) 판 시작 | 로그에 `직업 외형 적용 — … ABP=ABP_직업명` |

R-2 는 확률이 낮아 한 판으로는 안 잡힌다. 도둑을 상대로 여러 판 돌리거나
`DamageStealCount` 를 임시로 올려서 확인하는 편이 빠르다.

**⚠️ 줄 번호에 기대는 검사를 쓰지 말 것.** R-5 의 초안은 `sed -n '401,425p'` 로 범위를 잘랐는데,
그 위에 코드가 몇 줄만 들어가도 엉뚱한 곳을 보게 된다. 위 스크립트는 **함수 이름으로** 범위를 잡는다.
R-7 이 오탐으로 잡힌 것도 같은 이유였다 — 파일 전체에서 `grep -c` 한 결과가
**전혀 다른 함수의 가드**를 세고 있었다.

---

# 리뷰가 깨끗하다고 확인한 것

다시 볼 필요 없다. **다음 리뷰에서 중복으로 파지 않도록 남긴다.**

- **직업 12종 태그 개명** — 구 태그(`Warrior` `Rogue` `Cleric` `Ranger` `Paladin` `Berserker` `Alchemist`)
  잔존 0건. `Source/` · `Config/` · `Docs/` 전부. `BuildJobPool` 12개, `DT_BGCharacterRow` 12행,
  태그 문자열과 Row 이름 일치.
- **`ApplyModifiers` 의 새 필수 인자 `bAttackCard`** — C++ 호출자 2곳이 같은 식을 넘긴다.
  `BlueprintPure` 노드를 참조하는 `.uasset` 없음(기본값을 일부러 안 준 선택이 안전했다).
- **`AttackWeightBonusFailure`** — `PreAttributeChange` 의 0클램프 예외에 정확히 들어 있고
  `GetOutcomeWeightAttributes()` 에도 포함됨. `AttackKeepCardUseChance` 는 판정 비율이 아니라
  올바르게 **빠져** 있다.
- **`bCardUseRefunded`** — 대비(`KeepCardUse`)와 무투가가 겹쳐도 한 장에 한 번만 환급된다.
- **`EphemeralAttackCardId` 기본값 `"Card.Id.Bang"`** — DT Row 이름 규약(= 태그 전체 문자열)과 일치.
- **반사로 가해자가 죽는 경우** — `HandleSeatDeath` 가 규칙을 한 틱 미루므로 `ConsumeCard` 가 깨지지 않는다.
- **`GE_Stat_*` 애셋** — Knight · Healer · Monk 가 `MaxHealth` 와 `Health` 를 **둘 다** 갖고 있다.
  (Health 를 빼면 기사가 4/5 로, 치유사·수도사가 4/3 으로 시작하는 함정을 피했다.)
  Hunter `CardUseLimit`, Scout `DrawCount`. 나머지 7종은 생성자 기본값과 같아 빈 껍데기 — 정상이다.

---

# 이력

| 날짜 | 내용 |
|---|---|
| 2026-08-19 | 최초 작성. `/code-review high` 결과 8건을 등록(R-1 ~ R-8). 전수 재현 확인 과정에서 **R-4 는 이미 해결**(에디터에서 DT 수정), **R-7 은 오탐**(가드가 호출부에 있음)으로 판명. |
| 2026-08-19 | **R-1·R-2·R-3·R-5·R-8 수정.** `Build.bat BGEditor` 통과, 재확인 스크립트 전항목 통과. R-6 은 사용자 판단으로 **플레이 테스트 후 결정**으로 보류. 커밋·플레이 검증은 아직. |
| 2026-08-21 | **R-9 등록·수정.** ABP 구조 설계 중 발견 — `AnimClass` 가 메쉬에 묶여 있어 동작하지 않았다. 같은 날 **스켈레톤 단일화** 결정(아트 기획서 §아트 구현 특이사항 유지)으로 `AnimClass` 가 직업 개성의 주입구가 되면서 드러났다. 빌드 통과. |

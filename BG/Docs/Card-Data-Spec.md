# 카드 데이터 명세 (DT_BGCard)

작성 기준일: 2026-07-30 / 근거: `Game-Design-Document.md` v0.3, `Additional-Card-List.txt`

`Docs/Data/DT_BGCard.json` 을 임포트하면 이 표가 그대로 들어간다. 손으로 넣을 때는 아래 표를 본다.

---

## 임포트 절차

1. **먼저 `DT_BGCard` 를 복제해 백업한다.** JSON 임포트는 **기존 행을 전부 교체**한다.
2. 콘텐츠 브라우저에서 `DT_BGCard` 우클릭 → **Reimport / Import Data Table** → `Docs/Data/DT_BGCard.json` 선택

   > ⚠️ **첫 임포트는 반드시 `Reimport With New File` 을 쓴다.** 애셋 안에 저장된 소스 경로가
   > 아직 옛 이름(`Docs/Data/DT_BaamCard.json`)이라, 그냥 `Reimport` 를 누르면 없는 파일을
   > 찾다가 실패한다. 한 번 새 파일로 지정하고 나면 경로가 갱신되어 이후로는 `Reimport` 로 충분하다.

3. 에디터 재컴파일 없이 바로 반영된다. 실패하면 Output Log 에 행 단위 경고가 남는다.
4. 검증:
   ```
   BG.ExpectedDeckSize 64
   BG_DumpDeck        → 11 종류 / 64 장, InstanceId 중복 없음
   BG_PrintDeck       → 셔플된 순서
   ```
   아래 경고가 뜨면 그 행을 확인한다:
   - `[!! DT_BGCard 에 Row 없음]` — CardId 태그와 Row 이름이 어긋남
   - `[미구현: GA/GE 미지정]` — `AbilityEventTag` / `EquipEffect` 둘 다 비었음
   - `[!! 효과 데이터 없음 — OutcomeEffects 를 채우세요]` — **카드를 써도 효과가 0 이 된다**

> **임포트가 실패하거나 값이 비어 들어가면** 태그·중첩 구조체 문법 문제다. 그때는 아래 표대로
> 손으로 넣는 편이 빠르다. `FGameplayTag` 는 평문(`Card.Id.Bang`)으로 들어가고,
> `FGameplayTagContainer` 는 `GameplayTags` 배열 안에 `TagName` 을 넣는 형식이다.

## GA 매핑 (`Content/Core/BP_BGPlayerController` → `AbilityByCardId`)

**이걸 채우지 않으면 카드가 아무 일도 하지 않는다.** 서버 화면에 `GA 매핑 없음!` 이 뜬다.

**모든 갈색 카드가 `GA_BGCardEffects` 하나로 간다.** 카드마다 GA 를 만들지 않는다.

| CardId | GA |
|---|---|
| `Card.Id.Bang` | `GA_BGCardEffects` |
| `Card.Id.Beer` | `GA_BGCardEffects` |
| `Card.Id.CatBalou` | `GA_BGCardEffects` |
| `Card.Id.QuickStrike` | `GA_BGCardEffects` |
| `Card.Id.Rest` | `GA_BGCardEffects` |
| `Card.Id.ExposeWeakness` | `GA_BGCardEffects` |
| `Card.Id.Trap` | `GA_BGCardEffects` |
| `Card.Id.Prepare` | `GA_BGCardEffects` |
| `Card.Id.Brace` | `GA_BGCardEffects` |

장비 2종(`DwarfGloves` / `WitchCharm`)은 **매핑하지 않는다.** 파란 카드는 GA 를 발동하지 않고
`HandlePlayCard` 의 장착 경로로 빠진다.

---

## 판정 비율 (`OutcomeWeights`)

`-` 는 리스트에서 "해당 확률 0%" 를 뜻한다. 합이 100 일 필요는 없다(정규화됨).

| 카드 | 대실패 | 실패 | 성공 | 대성공 | 근거 |
|---|--:|--:|--:|--:|---|
| 뱅 | 5 | 20 | 65 | 10 | 기준선 |
| 맥주 | 0 | 10 | 80 | 10 | 자기 회복은 안정적으로 |
| **위협** | 0 | 0 | **100** | 0 | 리스트: 성공만 표기 |
| **속공** | 5 | 20 | 65 | 10 | 리스트: 4단계 모두 표기 |
| **휴식** | 0 | 20 | 70 | 10 | 리스트: 대실패 `-` |
| **약점 포착** | 10 | 0 | 75 | 15 | 리스트: 실패 `-`, 대실패는 "효과 없음" |
| **함정** | 0 | 20 | 65 | 15 | 리스트: 대실패 `-`, 실패는 "효과 없음" |
| **준비** | 0 | 0 | 85 | 15 | 리스트: 대실패·실패 `-` |
| **대비** | 0 | 0 | **100** | 0 | 리스트: 성공만 표기 |
| 장비 3종 | — | — | — | — | 파란 카드는 판정하지 않는다 |

> 리스트의 "효과 없음"(대실패/실패)과 `-`(0%)는 다르다. 전자는 **일어날 수 있지만 아무 일도 없는 것**,
> 후자는 **아예 일어나지 않는 것**이다. 위 표가 그 구분을 반영한다.

## 등급별 효과 (`OutcomeEffects`)

| 카드 | 실패 | 성공 | 대성공 |
|---|---|---|---|
| **뱅** | — | `DamageTarget 1` | `DamageTarget 2` |
| **맥주** | — | `HealSelf 1` | `HealSelf 2` |
| **위협** | — | `DiscardTargetRandom 1` | — |
| **속공** | `DrawSelf 1` | `DrawSelf 1` + `DamageTarget 1` | `DrawSelf 1` + `DamageTarget 1` + `RestoreCardUse 1` |
| **휴식** | `DrawSelf 1` | `DrawSelf 1` + `RestoreCardUse 1` | `DrawSelf 1` + `RestoreCardUse 2` |
| **약점 포착** | — | `ApplyStatus` `NextAttack.DamageBonus` **1** | 같은 상태 **2** |
| **함정** | — | `ApplyStatus` `NextCard.ForceFailure` **bToTarget** | `ForceCriticalFailure` **bToTarget** |
| **준비** | — | `ApplyStatus` `NextCard.ForceSuccess` | `ForceCriticalSuccess` |
| **대비** | — | `ApplyStatus` `NextCard.KeepCardUse` | — |

### 쓸 수 있는 연산 (`EBGCardEffectOp`)

| Op | 대상 | 설명 |
|---|---|---|
| `DamageTarget` | 대상 | 피해 |
| `HealSelf` | 자신 | 회복 |
| `DrawSelf` | 자신 | 덱에서 뽑기 |
| `DiscardTargetRandom` | 대상 **손패** | 무작위로 버리게 한다 |
| `StealTargetRandom` | 대상 **손패** | 무작위로 뺏어 내 손패로 |
| `DiscardTargetEquipRandom` | 대상 **장비** | 무작위로 떼어 버린다 |
| `StealTargetEquipRandom` | 대상 **장비** | 무작위로 떼어 **내 손패로** 가져온다 |
| `RestoreCardUse` | 자신 | 이번 턴 카드 사용 횟수 회복 |
| `ApplyStatus` | 자신/대상 | "다음 1회" 상태 부여 |

> **장비 연산은 손패를 건드리지 않는다.** 장비가 없는 상대에게 쓰면 아무 일도 일어나지 않는다
> (손패가 대신 날아가지 않는다). 반대도 마찬가지다.
>
> **훔친 장비는 장비 칸이 아니라 손패로 들어간다.** 바로 장착시키면 "장착은 카드 사용 횟수를
> 소비한다"(GDD §5.3)를 우회하게 되기 때문이다. 쓰려면 자기 턴에 다시 내야 한다.
>
> ⚠️ 새 연산은 enum **맨 뒤에** 추가할 것. 중간에 끼우면 DT 에 채워 둔 기존 값이 통째로 밀린다.

- **`ApplyStatus` 는 `Amount = 0` 도 유효하다** (강제 판정은 수치를 쓰지 않는다). 다른 Op 는 0 이면 건너뛴다.
- **함정만 `bToTarget = true`** 다. 나머지 상태는 자기 강화라 시전자에게 붙는다.
- **`OutcomeMagnitudes` 는 비활성이다.** 전부 0 으로 비워 둔다 — 채우면 어느 쪽이 적용되는지 혼란만 생긴다.
  (뱅의 피해가 안 들어간 문제가 이 이중 스키마 때문이었다. 이제 채울 필드는 `OutcomeEffects` 하나다.)

## 그 외 필드

| 카드 | TypeTag | 대상 지정 | 매수 | EquipEffect |
|---|---|:-:|--:|---|
| 뱅 | Brown | ✅ | 12 | — |
| 맥주 | Brown | | 6 | — |
| 위협 | Brown | ✅ | 6 | — |
| 속공 | Brown | ✅ | 8 | — |
| 휴식 | Brown | | 6 | — |
| 약점 포착 | Brown | | 6 | — |
| 함정 | Brown | ✅ | 6 | — |
| 준비 | Brown | | 5 | — |
| 대비 | Brown | | 5 | — |
| 드워프 장갑 | **Blue** | | 2 | `GE_Equip_DwarfGloves` |
| 마녀의 부적 | **Blue** | | 2 | `GE_Equip_WitchCharm` |
| 요정 갑옷 | **Blue** | | **0** | `GE_Equip_FairyArmor` |

**총 64장.** 대상 지정은 `TraitTags` 에 `Card.Trait.NeedsTargetSeat` 을 넣는다.

### `TraitTags` 로 켜는 카드 특성

한 카드에 여러 개를 넣을 수 있다. **비어 있는 것이 기본값**이다.

| 태그 | 의미 |
|---|---|
| `Card.Trait.NeedsTargetSeat` | 사용 시 대상 좌석을 지정해야 한다 |
| `Card.Trait.Attack` | **공격 카드다.** 직업 패시브의 적용 범위를 가른다 (아래 참고) |
| `Card.Trait.Ephemeral` | **일회용 카드다.** 덱에 들어가지 않고 규칙이 지급한다 (아래 참고) |
| `Card.Trait.Cinematic` | **사용 시 연출을 재생한다** (아트 기획서 "전투 연출") |
| `Card.Trait.Weapon` | 무기 — 파란 카드 중 1종만 장착 가능 |
| `Card.Trait.Reaction` | 응답 창 전용 (`빗나감!` 등). 응답 창은 미구현이라 현재 쓰이지 않는다 |
| `Card.Trait.RangeLimited` | 사거리 제약을 받는다. 사거리는 GDD §2.4 로 비활성이라 현재 쓰이지 않는다 |

> **`Card.Trait.Attack` 은 "대상을 지정한다" 와 다르다.** 함정은 대상을 고르지만 공격이 아니고,
> 광역기는 공격이지만 대상을 고르지 않는다. `NeedsTargetSeat` 으로 대신 판별하지 말 것.
>
> 이 태그를 읽는 곳은 직업 패시브 둘이다 — **용병**(공격 카드만 실패하지 않는다) 과
> **무투가**(공격 카드만 확률로 사용한도를 아낀다). 태그를 빼도 **카드 자체는 멀쩡히 동작하고
> 그 두 직업만 조용히 아무 일도 하지 않는다.** 현재 대상은 뱅·속공이다.
>
> **`Card.Trait.Cinematic` 은 규칙에 관여하지 않는다.** 판정도, 효과 적용도, 카드 소비도
> 태그와 무관하게 똑같이 일어난다 — 연출을 보여줄지만 정한다. 태그를 빼면 카드는 그대로
> 동작하고 연출만 사라진다.
>
> ⚠️ **파란(장비) 카드에는 붙여도 동작하지 않는다.** 장비는 판정 없이 장착 경로로 빠져
> 연출 훅에 닿지 않는다. 설계상 장착에는 연출을 두지 않기로 했다 — 붙이지 말 것.
>
> 어느 카드에 넣었는지는 `BG_DumpDeck` 이 `[연출]` 로 표시한다.
>
> **`Card.Trait.Ephemeral`** 은 세 가지를 바꾼다 — `BuildDeck` 이 `QuantityOfCard` 와 **무관하게**
> 이 카드를 건너뛴다(덱에 절대 안 들어간다) · 사용해도 카드 사용 한도를 소모하지 않는다 ·
> 사용하면 버린 패로 가지 않고 사라진다(덱 총량이 늘지 않는다).
>
> 지급은 `ABGGameState::GrantCards(PS, CardId, Count)` 하나로 한다. **일회용인지는 호출자가
> 정하지 않는다** — 그 함수가 이 태그를 보고 정한다. 그래서 "이 카드 N장 줘" 라고만 하면
> 카드 종류에 따라 알아서 갈린다(PvE 이벤트도 이 함수를 쓰면 된다).
>
> ⚠️ 일반 카드에 이 태그 없이 `GrantCards` 로 지급하면 **덱 총량이 늘어난다** — 막지는
> 않지만(PvE 에서 의도적으로 늘릴 수 있어야 한다) 경고를 남긴다. `QuantityOfCard` 를
> 0 으로만 두는 것으로는 부족하다 — 그건 "매수 0" 이라는 별개의 의미로 이미 쓰이고 있어
> (§2.2 요정 갑옷 참고) 태그 없이는 "일회용이라 0" 인지 "미구현이라 0" 인지 구분이 안 된다.
>
> **덱에 없으므로 `BG_DumpDeck` 의 덱 집계에 안 잡힌다.** 대신 같은 명령의 "덱 밖 카드"
> 구획이 이 카드군의 `OutcomeEffects`/`EquipEffect` 가 비었는지 따로 검사한다 — 이 구획이
> 없으면 "지급도 되고 사용도 되는데 효과만 0" 인 카드가 조용히 남는다.
>
> 현재 대상은 그림자 일격(`Card.Id.ShadowStrike`) 하나이며, 암살자의 `EphemeralCardOnCritical`
> (대성공 시 지급, `UBGDataSettings::EphemeralAttackCardId` 로 어떤 카드인지 설정)이 유일한 발급처다.

- **속공은 실패해도 대상이 필요하다** — 대상 선택이 카드 사용 시점에 일어나고 판정은 그 뒤다.
- **장비는 매수를 적게 뒀다.** 중복 장착이 금지되므로 손에 여러 장 있어도 한 장만 쓸 수 있다.
- **요정 갑옷의 방어 판정은 구현되어 있다** — `DamageNegateChance` 어트리뷰트를 25 올리고,
  피해가 들어가기 직전 `UGA_BGCardEffects::ApplyDamage` 가 서버에서 한 번 굴린다.
  **덱에 넣으려면 `QuantityOfCard` 만 올리면 된다** (몇 장을 넣을지는 밸런스 판단이라 0 으로 둠).
  판정 순서는 **공격 판정 성공 → 방어 판정** 이다 (GDD §14 미결 항목을 이렇게 확정).

---

## 테스트 시나리오

기대 동작을 확인할 순서다. 서버(호스트) 화면에 판정과 효과가 로그로 뜬다.

| # | 조작 | 기대 |
|---|---|---|
| 1 | 뱅을 대상에게 | `[판정] ... 성공` → 피해 1, 대상 HP 감소 |
| 2 | 카드 2장 사용 | 손패 잠김 + `카드 2 / 2` |
| 3 | 휴식 성공 | `1장 뽑기 + 사용한도 1 회복` → 카드를 더 쓸 수 있다 |
| 4 | 약점 포착 → 뱅 | 두 번째 카드의 피해가 +1 |
| 5 | 약점 포착 2연속 | **두 번째가 거부됨** (`이미 ... 상태입니다`) |
| 6 | 준비 → 아무 카드 | `[강제] ... 판정을 성공 으로 고정` |
| 7 | 준비 → **대비** | 대비는 성공 100% 라 강제가 의미 없지만 상태는 소모된다 |
| 8 | 함정을 상대에게 → 상대가 카드 사용 | 상대 카드가 실패로 고정 |
| 9 | 함정 → 상대가 **위협**(성공 100%) 사용 | `강제가 통하지 않습니다(해당 등급 비율 0)` — 리스트의 예외 규칙 |
| 10 | 대비 → 실패하는 카드 | `[대비] 실패했지만 카드 사용한도를 잃지 않았습니다` |
| 11 | 위협 | 상대 손패에서 무작위 1장이 사라짐 (상대 화면에서도 확인) |
| 12 | 드워프 장갑 장착 → 카드 사용 반복 | 실패 등급이 나오지 않는다 (대실패는 나옴) |
| 13 | 마녀의 부적 장착 | 대성공·대실패 빈도가 눈에 띄게 늘어난다 |
| 14 | 장비를 같은 종류로 2장 | **두 번째 거부** (`이미 장착 중`) |

9번과 12번이 이번에 새로 만든 규칙의 핵심 검증 지점이다.

## 재현 가능한 테스트

같은 판을 다시 돌리려면 시드를 고정한다. 로그에 `사용 시드 : N` 이 남으므로 그 값을 쓰면
셔플과 판정, 위협의 무작위 선택까지 모두 같게 재현된다.

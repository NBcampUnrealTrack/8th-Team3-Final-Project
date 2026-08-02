# 카드 데이터 명세 (DT_BaamCard)

작성 기준일: 2026-07-30 / 근거: `Game-Design-Document.md` v0.3, `Additional-Card-List.txt`

`Docs/Data/DT_BaamCard.json` 을 임포트하면 이 표가 그대로 들어간다. 손으로 넣을 때는 아래 표를 본다.

---

## 임포트 절차

1. **먼저 `DT_BaamCard` 를 복제해 백업한다.** JSON 임포트는 **기존 행을 전부 교체**한다.
2. 콘텐츠 브라우저에서 `DT_BaamCard` 우클릭 → **Reimport / Import Data Table** → `Docs/Data/DT_BaamCard.json` 선택
3. 에디터 재컴파일 없이 바로 반영된다. 실패하면 Output Log 에 행 단위 경고가 남는다.
4. 검증:
   ```
   Bang.ExpectedDeckSize 64
   Baam_DumpDeck      → 11 종류 / 64 장, InstanceId 중복 없음
   Baam_PrintDeck     → 셔플된 순서
   ```
   아래 경고가 뜨면 그 행을 확인한다:
   - `[!! DT_BaamCard 에 Row 없음]` — CardId 태그와 Row 이름이 어긋남
   - `[미구현: GA/GE 미지정]` — `AbilityEventTag` / `EquipEffect` 둘 다 비었음
   - `[!! 효과 데이터 없음 — OutcomeEffects 를 채우세요]` — **카드를 써도 효과가 0 이 된다**

> **임포트가 실패하거나 값이 비어 들어가면** 태그·중첩 구조체 문법 문제다. 그때는 아래 표대로
> 손으로 넣는 편이 빠르다. `FGameplayTag` 는 평문(`Card.Id.Bang`)으로 들어가고,
> `FGameplayTagContainer` 는 `GameplayTags` 배열 안에 `TagName` 을 넣는 형식이다.

## GA 매핑 (`BP_TestPlayerController` → `AbilityByCardId`)

**이걸 채우지 않으면 카드가 아무 일도 하지 않는다.** 서버 화면에 `GA 매핑 없음!` 이 뜬다.

**모든 갈색 카드가 `GA_BaamCardEffects` 하나로 간다.** 카드마다 GA 를 만들지 않는다.

| CardId | GA |
|---|---|
| `Card.Id.Bang` | `GA_BaamCardEffects` |
| `Card.Id.Beer` | `GA_BaamCardEffects` |
| `Card.Id.CatBalou` | `GA_BaamCardEffects` |
| `Card.Id.QuickStrike` | `GA_BaamCardEffects` |
| `Card.Id.Rest` | `GA_BaamCardEffects` |
| `Card.Id.ExposeWeakness` | `GA_BaamCardEffects` |
| `Card.Id.Trap` | `GA_BaamCardEffects` |
| `Card.Id.Prepare` | `GA_BaamCardEffects` |
| `Card.Id.Brace` | `GA_BaamCardEffects` |

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
| 요정 갑옷 | **Blue** | | **0** | — (G 미구현) |

**총 64장.** 대상 지정은 `TraitTags` 에 `Card.Trait.NeedsTargetSeat` 을 넣는다.

- **속공은 실패해도 대상이 필요하다** — 대상 선택이 카드 사용 시점에 일어나고 판정은 그 뒤다.
- **장비는 매수를 적게 뒀다.** 중복 장착이 금지되므로 손에 여러 장 있어도 한 장만 쓸 수 있다.
- **요정 갑옷은 `QuantityOfCard = 0`** — 방어 판정(G)이 없어 장착해도 효과가 없다. GDD §13.1 비활성 방식.

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

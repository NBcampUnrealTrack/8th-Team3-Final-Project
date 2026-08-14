// BANG! 스탯 (Prototype-Workflow.md §2 AttributeSet 표).
// 값 변경은 항상 서버에서 GE 로만 한다 (세터 직접 호출 금지 — 복제 어긋남).

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BGAttributeSet.generated.h"

// 접근자 4종 자동 생성 (Getter/ValueGetter/Setter/Initter). Team4 의 BaseAttributeSet 과 동일.
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class BG_API UBGAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBGAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// 생명력(=탄환). 손패 한도의 기준이기도 하다 (현재값 기준).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "BG")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, Health)

	// 최대 생명력. 캐릭터별 3~4, 보안관 +1.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "BG")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, MaxHealth)

	// 무기 사거리. Colt 1 / Schofield 2 / Remington 3 / Rev.Carabine 4 / Winchester 5.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WeaponRange, Category = "BG")
	FGameplayAttributeData WeaponRange;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, WeaponRange)

	// 거리 감소분 (Scope, Rose Doolan): 내가 남을 볼 때 -1.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DistanceReduction, Category = "BG")
	FGameplayAttributeData DistanceReduction;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, DistanceReduction)

	// 거리 증가분 (Mustang, Paul Regret): 남이 나를 볼 때 +1.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DistanceIncrease, Category = "BG")
	FGameplayAttributeData DistanceIncrease;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, DistanceIncrease)

	// 턴당 능동적으로 사용할 수 있는 카드 수 (GDD §7.1 확정 스탯). 초기값 2.
	//   - 버리기(Discard 페이즈)는 "카드 사용" 이 아니므로 소비하지 않는다.
	//   - 판정에 실패한 갈색 카드도 소비한다 (GDD §5.3).
	//   - 장비·캐릭터 능력·상태 효과가 GE 로 이 값을 올리거나 내릴 수 있다.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CardUseLimit, Category = "BG")
	FGameplayAttributeData CardUseLimit;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, CardUseLimit)

	// [비활성] 턴당 BANG! 사용 한도. GDD §3 에서 뱅! 전용 제한을 없앴다 — CardUseLimit 이 대신한다.
	//   선언만 유지하고 참조하지 않는다 (GDD §13.1 비활성 처리).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BangLimit, Category = "BG")
	FGameplayAttributeData BangLimit;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, BangLimit)

	// BANG! 을 막는 데 필요한 Missed! 장수. Slab the Killer → 2.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MissedRequired, Category = "BG")
	FGameplayAttributeData MissedRequired;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, MissedRequired)

	// 턴 시작 뽑기 장수 (기본 2).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DrawCount, Category = "BG")
	FGameplayAttributeData DrawCount;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, DrawCount)

	// 뽑기(Draw!) 판정 시 공개 장수 (Lucky Duke → 2장 중 선택).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DrawBangCount, Category = "BG")
	FGameplayAttributeData DrawBangCount;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, DrawBangCount)

	/**
	 * 들어오는 공격을 무효화할 확률(%). 0 = 방어 없음. 요정 갑옷이 25 를 더한다.
	 *
	 * GDD §7.1 의 확정 스탯 "방어 보정 — 자동 방어 판정" 이다. 하드코딩 25% 대신 어트리뷰트로 둔 이유:
	 *   · 다른 출처(직업 패시브·상태 효과)가 같은 값에 더할 수 있다
	 *   · 장비를 떼면 GE 가 회수되며 자동으로 원복된다
	 *   · UI 가 "현재 방어율" 을 읽어 보여줄 수 있다
	 *
	 * ⚠️ 판정은 **피해가 들어가기 직전 서버에서** 굴린다(UGA_BGCardEffects::ApplyDamage).
	 *    GDD §9.2 대로 플레이어 입력을 요구하지 않는 자동 판정이다.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageNegateChance, Category = "BG")
	FGameplayAttributeData DamageNegateChance;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, DamageNegateChance)

	// ── 판정 비율 보정 (GDD §7.1 확정 스탯 "성공률/대성공률 보정" / §4.2) ──
	//  장비·캐릭터·상태가 GE 로 이 값을 바꿔 카드 판정 확률을 조정한다.
	//
	//  적용 순서 (GDD §14 "보정 합산 방식" 을 여기서 확정):
	//    카드 기본 비율 × 승산(WeightMult*) + 가산(WeightBonus* + 행운) → 0 클램프 → 정규화
	//  승산을 먼저 적용하는 이유: 큰 음수 가산으로 등급을 "무효화" 하는 장비(드워프 장갑)가
	//  배율에 흔들리지 않아야 한다.
	//
	//  ⚠️ 가산 보정은 음수가 될 수 있다(무효화 용도). PreAttributeChange 의 0 클램프에서 제외된다.
	//  GDD §7.2 권고대로 최소 집합만 둔다 — 승산은 극단 등급(대실패/대성공)만 필요하다.

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WeightBonusCriticalFailure, Category = "BG|Outcome")
	FGameplayAttributeData WeightBonusCriticalFailure;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, WeightBonusCriticalFailure)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WeightBonusFailure, Category = "BG|Outcome")
	FGameplayAttributeData WeightBonusFailure;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, WeightBonusFailure)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WeightBonusSuccess, Category = "BG|Outcome")
	FGameplayAttributeData WeightBonusSuccess;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, WeightBonusSuccess)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WeightBonusCriticalSuccess, Category = "BG|Outcome")
	FGameplayAttributeData WeightBonusCriticalSuccess;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, WeightBonusCriticalSuccess)

	//  승산 — 기본 1.0. 마녀의 부적이 +1.0 을 더해 ×2 를 만든다.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WeightMultCriticalFailure, Category = "BG|Outcome")
	FGameplayAttributeData WeightMultCriticalFailure;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, WeightMultCriticalFailure)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WeightMultCriticalSuccess, Category = "BG|Outcome")
	FGameplayAttributeData WeightMultCriticalSuccess;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, WeightMultCriticalSuccess)

	// ── 직업 패시브용 보정 (GDD §7.1 "공격 보정 / 회복 보정") ────
	//
	//  전부 0 이 기본이고, 0 이면 그 훅은 아무 일도 하지 않는다.
	//  값은 직업 Row 의 PassiveEffects(Infinite GE)가 채운다 — 코드에는 숫자를 두지 않는다.

	/** 회복할 때마다 더해지는 회복량 (사제). 카드의 회복 수치에 가산된다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealBonus, Category = "BG|Job")
	FGameplayAttributeData HealBonus;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, HealBonus)

	/** 자기 턴이 시작될 때 회복하는 양 (치유사). 0 이면 회복하지 않는다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_TurnStartHeal, Category = "BG|Job")
	FGameplayAttributeData TurnStartHeal;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, TurnStartHeal)

	/**
	 * 피해를 줄 때 추가 피해가 터질 확률(%) (마법사). 0 = 없음.
	 * DamageNegateChance 와 같은 방식이다 — 피해 직전 서버가 한 번 굴린다.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageBonusChance, Category = "BG|Job")
	FGameplayAttributeData DamageBonusChance;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, DamageBonusChance)

	/**
	 * 위 확률이 터졌을 때 더해지는 피해량. **기본값이 1** 이다.
	 *
	 * 0 이 아니라 1 로 둔 이유: 패시브 GE 가 확률만 올려도 동작해야 한다. 0 이 기본이면
	 * 확률만 채운 GE 가 "확률은 터지는데 피해는 +0" 이 되어 조용히 아무 일도 안 한다.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageBonusAmount, Category = "BG|Job")
	FGameplayAttributeData DamageBonusAmount;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, DamageBonusAmount)

	/** 피해를 받을 때 가해자에게 되돌려주는 피해 (수도사). 0 = 반사 없음. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageReflect, Category = "BG|Job")
	FGameplayAttributeData DamageReflect;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, DamageReflect)

	/** 피해를 받을 때 가해자에게서 가져오는 손패 장수 (도둑). 0 = 없음. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageStealCount, Category = "BG|Job")
	FGameplayAttributeData DamageStealCount;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, DamageStealCount)

	/**
	 * 대성공 판정 때마다 받는 1회성 카드 장수 (암살자). 0 = 없음.
	 * 어느 카드를 받는지는 UBGDataSettings::EphemeralAttackCardId 가 정한다.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EphemeralCardOnCritical, Category = "BG|Job")
	FGameplayAttributeData EphemeralCardOnCritical;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, EphemeralCardOnCritical)

	// ── 공격 카드 한정 보정 (직업 패시브) ────────────────────────
	//
	//  위 보정들이 "모든 카드" 에 걸리는 것과 달리, 이 둘은 Card.Trait.Attack 이 붙은
	//  카드에만 걸린다. 범위를 어트리뷰트 이름에 박아 둔 이유는 같은 보정을 두 벌 두는
	//  방식이 확장성이 낮기 때문이다 — 범위별 보정이 더 필요해지면 FBGCardRow::ModifierTags
	//  (GDD §4.3 "보정 허용 태그", 아직 미구현)를 살려 일반화할 것.

	/**
	 * 공격 카드 판정에만 더해지는 실패 가산 (용병).
	 *
	 * 큰 음수(-1000)를 넣으면 공격 카드의 실패 비율이 0 이 된다. 그러면 함정
	 * (Status.NextCard.ForceFailure)도 자동으로 막힌다 — "해당 등급 비율이 0 이면 강제할 수 없다"
	 * 는 기존 규칙이 그대로 적용되기 때문이다. 우선순위 규칙을 따로 만들지 않은 이유가 이것이다.
	 *
	 * ⚠️ WeightBonusFailure 와 마찬가지로 음수를 허용해야 한다(PreAttributeChange 의 0 클램프 제외).
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackWeightBonusFailure, Category = "BG|Outcome")
	FGameplayAttributeData AttackWeightBonusFailure;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, AttackWeightBonusFailure)

	/**
	 * 공격 카드를 썼을 때 카드 사용 한도를 소모하지 않을 확률(%) (무투가). 0 = 항상 소모.
	 *
	 * DamageNegateChance 와 같은 방식이다 — 어트리뷰트에 확률을 담고 서버가 한 번 굴린다.
	 * 굴림은 덱 셔플과 분리된 판정 스트림(UBGDiceComponent::RandomIndex)을 쓴다.
	 *
	 * ⚠️ 판정 비율이 아니라 사용한도 회계에 쓰인다 — GetOutcomeWeightAttributes() 에 넣지 말 것.
	 *    넣으면 이 값이 바뀔 때마다 손패 확률 UI 가 이유 없이 다시 그려진다.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackKeepCardUseChance, Category = "BG|Outcome")
	FGameplayAttributeData AttackKeepCardUseChance;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, AttackKeepCardUseChance)

	/**
	 * 위 6종 + 공격 한정 가산. UBGDiceComponent::ApplyModifiers 가 읽는 것과 같은 집합이다.
	 *
	 * 확률 UI 는 이 값이 바뀌면 손패를 다시 그려야 한다(ABGPlayerController). 목록을 손으로
	 * 나열하면 보정을 하나 추가했을 때 UI 만 조용히 낡으므로 여기 한 곳에서만 관리한다.
	 * ⚠️ 보정 어트리뷰트를 추가하면 ApplyModifiers 와 이 목록에 함께 넣을 것.
	 */
	static TArray<FGameplayAttribute> GetOutcomeWeightAttributes();

	// ── [비활성] 능력치(RPG 스탯) 4종 ────────────────────────────
	//
	//  ⚠️ 네 스탯 모두 어떤 규칙도 읽지 않는다 (GDD §7.1 / §13.1).
	//     이 넷은 §7.1 의 확정 스탯 목록에 없다 — 확정된 것은 카드 사용 한도와
	//     성공률·대성공률·방어·공격·회복·드로우 보정이고, 힘·민첩·지능·행운은 그 축을
	//     RPG 능력치로 다시 표현한 별개 계층이라 중복이다.
	//     선언과 캐릭터 GE 세팅은 남기되 참조를 끊었다.
	//
	//     끊은 이유:
	//       힘·지능 — 지능 1(= round(1 × 0.5) = 1)이 피해 1 을 통째로 상쇄해
	//                 "성공했는데 피해 0" 이 되었다. 카드 수치가 곧 피해여야
	//                 §10 효과 표시와 §11 밸런스 지표가 성립한다.
	//       행운   — 판정 비율에 섞이면 DT 비율이 곧 표시 확률이 아니게 된다.
	//       민첩   — 읽던 곳이 비활성 GA_Missed 뿐이라 연쇄 비활성.
	//
	//     되살리는 법: 확률 보정은 WeightBonus* / WeightMult* 어트리뷰트로 환산하고,
	//     피해 보정은 GA_BGCardEffects::ApplyDamage 에서 다시 곱한다.

	// 힘 — 뱅 데미지 배율. 1당 GA_Bang 의 StrengthDamageMult 만큼 피해 증가.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Strength, Category = "BG|Stat")
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, Strength)

	// 민첩 — 회피(Missed!) 판정 굴림에 가산 (행운과 별개로 추가).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Agility, Category = "BG|Stat")
	FGameplayAttributeData Agility;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, Agility)

	// 지능 — 받는 데미지 경감. 1당 GA_Bang 의 IntelligenceMitigation 만큼 피해 감소.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Intelligence, Category = "BG|Stat")
	FGameplayAttributeData Intelligence;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, Intelligence)

	// 행운 — 모든 판정 굴림에 가산 (뱅 공격·회피 공통).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Luck, Category = "BG|Stat")
	FGameplayAttributeData Luck;
	ATTRIBUTE_ACCESSORS(UBGAttributeSet, Luck)

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeaponRange(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DistanceReduction(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DistanceIncrease(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CardUseLimit(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_BangLimit(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MissedRequired(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DrawCount(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DrawBangCount(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageNegateChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightBonusCriticalFailure(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightBonusFailure(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightBonusSuccess(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightBonusCriticalSuccess(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightMultCriticalFailure(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightMultCriticalSuccess(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackWeightBonusFailure(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackKeepCardUseChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_TurnStartHeal(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageBonusChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageBonusAmount(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageReflect(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageStealCount(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_EphemeralCardOnCritical(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Strength(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Agility(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Intelligence(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Luck(const FGameplayAttributeData& OldValue);
};

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

	// ── 판정 비율 보정 (GDD §7.2 / §4.2) ─────────────────────────
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

	// ── [비활성] 능력치(RPG 스탯) 4종 ────────────────────────────
	//
	//  ⚠️ 네 스탯 모두 어떤 규칙도 읽지 않는다 (GDD §7.1 / §13.1).
	//     §7.1 의 확정 스탯은 CardUseLimit 하나뿐이고, §7.2 도 초기 프로토타입에서는
	//     스탯을 최소로 두라고 권한다. 선언과 캐릭터 GE 세팅은 남기되 참조를 끊었다.
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
	UFUNCTION() void OnRep_WeightBonusCriticalFailure(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightBonusFailure(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightBonusSuccess(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightBonusCriticalSuccess(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightMultCriticalFailure(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeightMultCriticalSuccess(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Strength(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Agility(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Intelligence(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Luck(const FGameplayAttributeData& OldValue);
};

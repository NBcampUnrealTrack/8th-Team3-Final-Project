#include "GAS/BGAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Core/BGCharacter.h"
#include "Net/UnrealNetwork.h"

UBGAttributeSet::UBGAttributeSet()
{
	// 안전한 기본값 — 실제 값은 캐릭터 Row 의 DefaultAttributeGE 로 덮어쓴다.
	InitHealth(4.f);
	InitMaxHealth(4.f);
	InitWeaponRange(1.f);
	InitDistanceReduction(0.f);
	InitDistanceIncrease(0.f);
	InitCardUseLimit(2.f);   // GDD §7.1 초기값
	InitBangLimit(1.f);
	InitMissedRequired(1.f);
	InitDrawCount(2.f);
	InitDrawBangCount(1.f);
	InitDamageNegateChance(0.f);   // 방어 없음. 요정 갑옷을 장착하면 GE 가 25 를 더한다.

	// 판정 비율 보정 — 가산은 0(보정 없음), 승산은 1.0(그대로).
	InitWeightBonusCriticalFailure(0.f);
	InitWeightBonusFailure(0.f);
	InitWeightBonusSuccess(0.f);
	InitWeightBonusCriticalSuccess(0.f);
	InitWeightMultCriticalFailure(1.f);
	InitWeightMultCriticalSuccess(1.f);

	// 공격 카드 한정 보정 — 둘 다 0(보정 없음). 직업 패시브 GE 가 채운다.
	InitAttackWeightBonusFailure(0.f);
	InitAttackKeepCardUseChance(0.f);

	// 능력치 — 기본 0 (보정 없음). 실제 값은 캐릭터 Row 의 GE 로 세팅한다.
	InitStrength(0.f);
	InitAgility(0.f);
	InitIntelligence(0.f);
	InitLuck(0.f);
}

TArray<FGameplayAttribute> UBGAttributeSet::GetOutcomeWeightAttributes()
{
	//	UBGDiceComponent::ApplyModifiers 가 읽는 것과 같은 집합.
	//	AttackWeightBonusFailure 는 공격 카드에만 걸리지만, 손패에는 공격 카드도 함께 있으므로
	//	이 값이 바뀌면 확률 표시를 다시 그려야 한다.
	//	AttackKeepCardUseChance 는 판정 비율이 아니라 사용한도 회계라 넣지 않는다.
	return {
		GetAttackWeightBonusFailureAttribute(),
		GetWeightBonusCriticalFailureAttribute(),
		GetWeightBonusFailureAttribute(),
		GetWeightBonusSuccessAttribute(),
		GetWeightBonusCriticalSuccessAttribute(),
		GetWeightMultCriticalFailureAttribute(),
		GetWeightMultCriticalSuccessAttribute()
	};
}

void UBGAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// REPNOTIFY_Always: 같은 값으로 재적용돼도 OnRep 을 태워 UI 가 항상 갱신되도록.
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, Health,            COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, MaxHealth,         COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, WeaponRange,       COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, DistanceReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, DistanceIncrease,  COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, CardUseLimit,      COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, BangLimit,         COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, MissedRequired,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, DrawCount,         COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, DrawBangCount,     COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, DamageNegateChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, WeightBonusCriticalFailure, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, WeightBonusFailure,         COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, WeightBonusSuccess,         COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, WeightBonusCriticalSuccess, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, WeightMultCriticalFailure,  COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, WeightMultCriticalSuccess,  COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, AttackWeightBonusFailure,   COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, AttackKeepCardUseChance,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, Strength,          COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, Agility,           COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, Intelligence,      COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBGAttributeSet, Luck,              COND_None, REPNOTIFY_Always);
}

void UBGAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 스탯은 음수가 되지 않는다. 거리 보정치도 0 미만은 무의미.
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetWeightBonusCriticalFailureAttribute()
		|| Attribute == GetWeightBonusFailureAttribute()
		|| Attribute == GetWeightBonusSuccessAttribute()
		|| Attribute == GetWeightBonusCriticalSuccessAttribute()
		|| Attribute == GetAttackWeightBonusFailureAttribute())
	{
		//	가산 비율 보정은 음수를 허용한다 — 큰 음수로 등급을 무효화하는 장비가 있다
		//	(드워프 장갑: 실패 비율을 0 으로). 0 클램프하면 그 카드가 동작하지 않는다.
		//	최종 비율은 정규화 단계에서 0 아래로 내려가지 않도록 처리된다.
		//
		//	⚠️ 공격 한정 가산(용병)도 반드시 여기 있어야 한다. 빠뜨리면 -1000 이 0 으로 잘려
		//	   패시브가 붙어 있는데 아무 일도 일어나지 않는다 — 원인이 어디에도 보이지 않는다.
	}
	else
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UBGAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// GE(피해/회복) 적용 후 Health 를 [0, MaxHealth] 로 최종 클램프.
	if (Data.EvaluatedData.Attribute != GetHealthAttribute())
	{
		return;
	}

	SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));

	if (GetHealth() > 0.f)
	{
		return;
	}

	// AttributeSet 은 "값이 0 이 됐다" 까지만 안다.
	// 사망이라는 규칙 사건(카드 정리·보상/벌칙·승패·턴)은 캐릭터/룰 계층이 처리한다 —
	// 여기에 규칙을 넣으면 나중에 ASC 를 PlayerState 로 옮길 때 통째로 끌려다닌다.
	if (ABGCharacter* OwnerChar = Cast<ABGCharacter>(GetOwningActor()))
	{
		// 가해자는 피해 GE 의 Instigator (GA_Bang 이 Context.AddInstigator 로 넣는다).
		AActor* Killer = Data.EffectSpec.GetEffectContext().GetInstigator();
		OwnerChar->NotifyHealthDepleted(Killer);
	}
}

void UBGAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, Health, OldValue);
}
void UBGAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, MaxHealth, OldValue);
}
void UBGAttributeSet::OnRep_WeaponRange(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, WeaponRange, OldValue);
}
void UBGAttributeSet::OnRep_DistanceReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, DistanceReduction, OldValue);
}
void UBGAttributeSet::OnRep_DistanceIncrease(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, DistanceIncrease, OldValue);
}
void UBGAttributeSet::OnRep_CardUseLimit(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, CardUseLimit, OldValue);
}
void UBGAttributeSet::OnRep_BangLimit(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, BangLimit, OldValue);
}
void UBGAttributeSet::OnRep_MissedRequired(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, MissedRequired, OldValue);
}
void UBGAttributeSet::OnRep_DrawCount(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, DrawCount, OldValue);
}
void UBGAttributeSet::OnRep_DrawBangCount(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, DrawBangCount, OldValue);
}
void UBGAttributeSet::OnRep_DamageNegateChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, DamageNegateChance, OldValue);
}
void UBGAttributeSet::OnRep_WeightBonusCriticalFailure(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, WeightBonusCriticalFailure, OldValue);
}
void UBGAttributeSet::OnRep_WeightBonusFailure(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, WeightBonusFailure, OldValue);
}
void UBGAttributeSet::OnRep_WeightBonusSuccess(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, WeightBonusSuccess, OldValue);
}
void UBGAttributeSet::OnRep_WeightBonusCriticalSuccess(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, WeightBonusCriticalSuccess, OldValue);
}
void UBGAttributeSet::OnRep_WeightMultCriticalFailure(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, WeightMultCriticalFailure, OldValue);
}
void UBGAttributeSet::OnRep_WeightMultCriticalSuccess(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, WeightMultCriticalSuccess, OldValue);
}
void UBGAttributeSet::OnRep_AttackWeightBonusFailure(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, AttackWeightBonusFailure, OldValue);
}
void UBGAttributeSet::OnRep_AttackKeepCardUseChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, AttackKeepCardUseChance, OldValue);
}
void UBGAttributeSet::OnRep_Strength(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, Strength, OldValue);
}
void UBGAttributeSet::OnRep_Agility(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, Agility, OldValue);
}
void UBGAttributeSet::OnRep_Intelligence(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, Intelligence, OldValue);
}
void UBGAttributeSet::OnRep_Luck(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBGAttributeSet, Luck, OldValue);
}

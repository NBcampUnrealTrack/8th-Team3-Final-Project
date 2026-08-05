#include "GAS/Effect/GE_BGEquip.h"
#include "GAS/BGAttributeSet.h"

namespace
{
	/** Infinite + 지정 어트리뷰트에 상수 가산 모디파이어 하나. */
	void AddConstantModifier(TArray<FGameplayModifierInfo>& Modifiers,
		const FGameplayAttribute& Attribute, float Value)
	{
		FGameplayModifierInfo Info;
		Info.Attribute = Attribute;
		Info.ModifierOp = EGameplayModOp::Additive;
		Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		Modifiers.Add(Info);
	}
}

UGE_Equip_DwarfGloves::UGE_Equip_DwarfGloves()
{
	//	장착 중 계속 유지된다. 해제하면 RemoveActiveGameplayEffect 로 되돌아간다.
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	//	실패 비율을 사실상 0 으로. 어떤 카드의 기본 실패 비율보다도 크게 잡는다.
	//	(가산 보정은 PreAttributeChange 의 0 클램프에서 제외되어 음수가 유지된다)
	AddConstantModifier(Modifiers, UBGAttributeSet::GetWeightBonusFailureAttribute(), -1000.f);
}

UGE_Equip_WitchCharm::UGE_Equip_WitchCharm()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	//	기본 1.0 에 +1.0 → ×2.
	AddConstantModifier(Modifiers, UBGAttributeSet::GetWeightMultCriticalSuccessAttribute(), 1.f);
	AddConstantModifier(Modifiers, UBGAttributeSet::GetWeightMultCriticalFailureAttribute(), 1.f);
}

UGE_Equip_FairyArmor::UGE_Equip_FairyArmor()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	//	방어율 +25%p. 가산이라 다른 출처(직업 패시브 등)와 겹치면 합쳐진다.
	//	두 장을 겹쳐 끼는 것은 EquipCard 의 중복 장착 금지가 막는다.
	AddConstantModifier(Modifiers, UBGAttributeSet::GetDamageNegateChanceAttribute(), 25.f);
}

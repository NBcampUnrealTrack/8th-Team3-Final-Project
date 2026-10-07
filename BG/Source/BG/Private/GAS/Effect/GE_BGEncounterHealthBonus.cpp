#include "GAS/Effect/GE_BGEncounterHealthBonus.h"
#include "GAS/BGAttributeSet.h"

UGE_BGEncounterHealthBonus::UGE_BGEncounterHealthBonus()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	//	보안관 보너스와 같은 이유로 둘 다 올린다. MaxHealth 만 올리면 현재 체력은 그대로라
	//	실제로 더 버티지 못하고 천장만 높아진다.
	FGameplayModifierInfo MaxHealthMod;
	MaxHealthMod.Attribute = UBGAttributeSet::GetMaxHealthAttribute();
	MaxHealthMod.ModifierOp = EGameplayModOp::Additive;
	MaxHealthMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(HealthPerStack));
	Modifiers.Add(MaxHealthMod);

	FGameplayModifierInfo HealthMod;
	HealthMod.Attribute = UBGAttributeSet::GetHealthAttribute();
	HealthMod.ModifierOp = EGameplayModOp::Additive;
	HealthMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(HealthPerStack));
	Modifiers.Add(HealthMod);
}

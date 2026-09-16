#include "GAS/Effect/GE_BGSheriffBonus.h"
#include "GAS/BGAttributeSet.h"

UGE_BGSheriffBonus::UGE_BGSheriffBonus()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// MaxHealth 만 올리면 현재 체력은 그대로라 "가득 찬 +1"이 아니라 "천장만 높아진 상태"가
	// 된다. 보안관이 실제로 한 대 더 버티게 하려면 Health 도 같이 올려야 한다.
	FGameplayModifierInfo MaxHealthMod;
	MaxHealthMod.Attribute = UBGAttributeSet::GetMaxHealthAttribute();
	MaxHealthMod.ModifierOp = EGameplayModOp::Additive;
	MaxHealthMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f));
	Modifiers.Add(MaxHealthMod);

	FGameplayModifierInfo HealthMod;
	HealthMod.Attribute = UBGAttributeSet::GetHealthAttribute();
	HealthMod.ModifierOp = EGameplayModOp::Additive;
	HealthMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f));
	Modifiers.Add(HealthMod);
}

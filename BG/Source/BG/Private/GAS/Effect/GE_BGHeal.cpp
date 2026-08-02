#include "GAS/Effect/GE_BGHeal.h"
#include "GAS/BGAttributeSet.h"
#include "BGGameplayTags.h"

UGE_BGHeal::UGE_BGHeal()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Health += SetByCaller(SetByCaller.Heal). 회복은 양수.
	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UBGAttributeSet::GetHealthAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SBC;
	SBC.DataTag = BG::SetByCaller::Heal.GetTag();
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SBC);

	Modifiers.Add(ModInfo);
}

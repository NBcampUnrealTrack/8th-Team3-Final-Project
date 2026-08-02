#include "GAS/Effect/GE_BGDamage.h"
#include "GAS/BGAttributeSet.h"
#include "BGGameplayTags.h"

UGE_BGDamage::UGE_BGDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Health += SetByCaller(SetByCaller.Damage). 피해는 음수로 넣어 감소시킨다.
	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UBGAttributeSet::GetHealthAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SBC;
	SBC.DataTag = BG::SetByCaller::Damage.GetTag();
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SBC);

	Modifiers.Add(ModInfo);
}

// ⚠️ [비활성] AbilityByCardId 에 매핑하지 말 것 (GDD §13.1).
//   결투는 두 좌석이 번갈아 응답해야 하는데 응답 창 시스템이 아직 없다.
//   본문이 TODO 스텁이라 매핑하면 카드만 소비되고 아무 일도 일어나지 않는다.
//   정식 경로는 UGA_BGCardEffects + DT_BGCard 의 OutcomeEffects 하나뿐이다.

#include "GAS/Ability/Legacy/GA_Duel.h"
#include "BGGameplayTags.h"
#include "AbilitySystemComponent.h"

UGA_Duel::UGA_Duel()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BG::Ability::Duel.GetTag());
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(BG::State::Dead.GetTag());
}

void UGA_Duel::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// TODO(§3): 대상부터 시작해 두 좌석을 번갈아 Response.Allow.Bang 요청.
	//           먼저 Pass(BANG! 못 냄)한 쪽에 GE_BGDamage(DamageAmount).
	UE_LOG(LogTemp, Log, TEXT("[BG] GA_Duel — 피해 %d"), DamageAmount);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

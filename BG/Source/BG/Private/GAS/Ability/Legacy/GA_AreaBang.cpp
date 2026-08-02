// ⚠️ [비활성] AbilityByCardId 에 매핑하지 말 것 (GDD §13.1).
//   개틀링/인디언은 "전원에게 순차 응답 요청" 이 필요한데 응답 창 시스템이 아직 없다.
//   본문이 TODO 스텁이라 매핑하면 카드만 소비되고 아무 일도 일어나지 않는다.
//   정식 경로는 UGA_BGCardEffects + DT_BGCard 의 OutcomeEffects 하나뿐이다.

#include "GAS/Ability/Legacy/GA_AreaBang.h"
#include "BGGameplayTags.h"
#include "AbilitySystemComponent.h"

UGA_AreaBang::UGA_AreaBang()
{
	// 기본값 = 개틀링(Missed! 로 방어). 인디언은 카드 데이터에서 Response.Allow.Bang 으로.
	ResponseAllow = BG::Response::Allow::Missed.GetTag();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BG::Ability::AreaBang.GetTag());
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(BG::State::Dead.GetTag());
}

void UGA_AreaBang::ActivateAbility(
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

	// TODO(§3): 시전자 기준 시계방향 살아있는 전원에게 ResponseAllow 요청을 순차 push.
	//           막지 못한 좌석마다 GE_BGDamage(DamageAmount) 적용.
	UE_LOG(LogTemp, Log, TEXT("[BG] GA_AreaBang — 응답=%s 피해=%d"),
		*ResponseAllow.ToString(), DamageAmount);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

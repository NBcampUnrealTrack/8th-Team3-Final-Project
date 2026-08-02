// ⚠️ [비활성] AbilityByCardId 에 매핑하지 말 것 (GDD §13.1).
//   잡화점은 카드를 공개해 두고 시계방향으로 고르는 선택 UI 가 필요한데 아직 없다.
//   본문이 TODO 스텁이라 매핑하면 카드만 소비되고 아무 일도 일어나지 않는다.
//   정식 경로는 UGA_BGCardEffects + DT_BGCard 의 OutcomeEffects 하나뿐이다.

#include "GAS/Ability/Legacy/GA_GeneralStore.h"
#include "BGGameplayTags.h"
#include "AbilitySystemComponent.h"

UGA_GeneralStore::UGA_GeneralStore()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BG::Ability::GeneralStore.GetTag());
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(BG::State::Dead.GetTag());
}

void UGA_GeneralStore::ActivateAbility(
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

	// TODO(§3): 살아있는 인원 수만큼 덱에서 공개 → 현재 플레이어부터 시계방향으로
	//           PickFromRevealed 응답을 순차 요청. 고른 카드는 각자 손으로, 나머지 정리.
	UE_LOG(LogTemp, Log, TEXT("[BG] GA_GeneralStore — 공개 후 순차 선택"));

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

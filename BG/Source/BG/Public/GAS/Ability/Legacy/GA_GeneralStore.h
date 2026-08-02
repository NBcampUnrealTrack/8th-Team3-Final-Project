// 잡화점(General Store) — 살아있는 인원 수만큼 카드를 공개하고,
// 현재 플레이어부터 시계방향으로 한 장씩 순서대로 고른다.

#pragma once

#include "CoreMinimal.h"
#include "GAS/BGGameplayAbility.h"
#include "GA_GeneralStore.generated.h"

UCLASS()
class BG_API UGA_GeneralStore : public UBGGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_GeneralStore();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};

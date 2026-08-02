// 술집(Saloon) — 살아있는 전원이 Health 1 회복.

#pragma once

#include "CoreMinimal.h"
#include "GAS/BGGameplayAbility.h"
#include "GA_Saloon.generated.h"

UCLASS()
class BG_API UGA_Saloon : public UBGGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Saloon();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// 전원 회복량.
	UPROPERTY(EditDefaultsOnly, Category = "BG")
	int32 HealAmount = 1;
};

// 카드 수급 — 역마차(2장) / 웰스파고(3장). 시전자가 덱에서 DrawCount 장을 뽑는다.

#pragma once

#include "CoreMinimal.h"
#include "GAS/BGGameplayAbility.h"
#include "GA_DrawN.generated.h"

UCLASS()
class BG_API UGA_DrawN : public UBGGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_DrawN();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// 뽑을 장수 (역마차 2 / 웰스파고 3). 카드 데이터에서 지정.
	UPROPERTY(EditDefaultsOnly, Category = "BG")
	int32 DrawCount = 2;
};

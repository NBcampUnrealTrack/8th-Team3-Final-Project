// 광역 공격 — 개틀링 / 인디언 습격. 시전자 제외 살아있는 전원에게 순차로 응답을 요구한다.
//   개틀링 : 각자 Missed! 로 막지 못하면 피해 1  → ResponseAllow = Response.Allow.Missed
//   인디언 : 각자 Bang! 을 내지 못하면 피해 1    → ResponseAllow = Response.Allow.Bang

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/BGGameplayAbility.h"
#include "GA_AreaBang.generated.h"

UCLASS()
class BG_API UGA_AreaBang : public UBGGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AreaBang();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// 각 대상이 낼 수 있는 응답 (개틀링=Missed / 인디언=Bang). 카드 데이터에서 지정.
	UPROPERTY(EditDefaultsOnly, Category = "BG")
	FGameplayTag ResponseAllow;

	// 막지 못한 대상이 받는 피해.
	UPROPERTY(EditDefaultsOnly, Category = "BG")
	int32 DamageAmount = 1;
};

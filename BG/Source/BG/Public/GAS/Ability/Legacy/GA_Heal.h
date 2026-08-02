// 회복 — Beer(자신). 판정은 서버가 카드 DT(DT_BGCard)로 끝내고, 이 GA 는 결과만 실행한다.
// 등급별 회복량은 카드 DT 의 OutcomeMagnitudes 에서 온다(GDD §4.3).
// Saloon(전원 회복)은 GA_Saloon 이 담당.

#pragma once

#include "CoreMinimal.h"
#include "GAS/BGGameplayAbility.h"
#include "GA_Heal.generated.h"

UCLASS()
class BG_API UGA_Heal : public UBGGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Heal();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// [비활성] 판정 없이 발동됐을 때 쓰던 폴백 회복량. 실제 회복량은 카드 DT 의 OutcomeMagnitudes 다.
	//   선언만 유지하고 참조하지 않는다 (GDD §13.1). 사유·복구는 GA_Heal.cpp 주석 참고.
	UPROPERTY(EditDefaultsOnly, Category = "BG")
	int32 HealAmount = 1;
};

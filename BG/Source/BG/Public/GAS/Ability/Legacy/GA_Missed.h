// 빗나감!(Missed!) — 응답 전용. BANG!/개틀링에 대한 반응으로만 발동된다 (능동 사용 X).
//   서버 주사위 판정을 성공/실패로 압축한다: 성공·대성공 → 회피 성공(무효화), 실패·대실패 → 실패(피격).
//   행운 + 민첩 이 굴림 보정으로 들어간다(잘 피함). 응답 창(Resolution) 연동은 §3에서 확장.

#pragma once

#include "CoreMinimal.h"
#include "GAS/BGGameplayAbility.h"
#include "GA_Missed.generated.h"

UCLASS()
class BG_API UGA_Missed : public UBGGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Missed();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// 회피 판정이 확정됐을 때(연출·응답 제출용) 방송. 서버에서 호출.
	UFUNCTION(BlueprintImplementableEvent, Category = "BG")
	void OnDodgeResolved(bool bSuccess);
};

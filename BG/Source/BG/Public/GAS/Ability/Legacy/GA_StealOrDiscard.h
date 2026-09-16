// 위협(캣벌로우) — 대상 손패에서 카드를 무작위로 버린다.
//   Additional-Card-List: "성공: 상대의 핸드에서 카드를 1장 무작위로 버린다" (성공 100%)
//
// GDD §2.2 대로 대상의 응답을 기다리지 않는다. 어느 카드를 버릴지는 서버가 무작위로 정하고
// 즉시 확정한다 — 원작의 "손패 뒷면 중 하나를 지목" 절차를 자동화한 것이다.
//
// 버릴 장수는 카드 데이터의 등급별 수치로 전달됐었다(그 필드는 삭제됨).
//   → 수치 0(실패/대실패)이면 아무 일도 일어나지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "GAS/BGGameplayAbility.h"
#include "GA_StealOrDiscard.generated.h"

UCLASS()
class BG_API UGA_StealOrDiscard : public UBGGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_StealOrDiscard();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// false = 버리기(위협 → 버린 패) / true = 훔치기(당황 계열 → 시전자 손으로).
	//
	// 기본값 false 는 위협용이다 — 이 클래스를 AbilityByCardId 에 그대로 지정하면 동작한다.
	// 훔치기 변형이 필요해지면 이 값만 true 로 둔 BP 서브클래스를 만들어 매핑한다.
	UPROPERTY(EditDefaultsOnly, Category = "BG")
	bool bSteal = false;

	// 판정 결과와 실제로 옮긴 장수를 알린다(연출·UI용). 서버에서 호출.
	UFUNCTION(BlueprintImplementableEvent, Category = "BG")
	void OnStealOrDiscardResolved(int32 MovedCount, AActor* Target);
};

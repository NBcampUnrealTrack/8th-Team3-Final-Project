#include "GAS/BGGameplayAbility.h"
#include "Core/BGPlayerState.h"
#include "GameFramework/Pawn.h"

UBGGameplayAbility::UBGGameplayAbility()
{
	// 서버 권위 실행 — 클라 예측 없음 (md §1.2).
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

ABGPlayerState* UBGGameplayAbility::GetBGPlayerState(AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn ? Pawn->GetPlayerState<ABGPlayerState>() : nullptr;
}

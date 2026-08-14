//TRPlayerController.cpp


#include "Player/TRPlayerController.h"
#include "Core/TRGameState.h"
#include "Debug/TRGameplayLog.h"

void ATRPlayerController::TR_AddResource(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	ServerCheatAddResource(Amount);
#endif
}

void ATRPlayerController::ServerCheatAddResource_Implementation(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	ATRGameState* GS = IsValid(World) ? World->GetGameState<ATRGameState>() : nullptr;
	if (!IsValid(GS))
	{
		return;
	}
	GS->AddMonsterResource(Amount);
	UE_LOG(LogTRGameplay, Log, TEXT("Cheat: MonsterResource +%d (now %d)"), Amount, GS->GetMonsterResource());
#endif
}

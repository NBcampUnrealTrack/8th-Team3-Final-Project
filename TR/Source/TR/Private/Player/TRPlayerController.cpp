//TRPlayerController.cpp


#include "Player/TRPlayerController.h"

#include "Core/TRGameMode.h"
#include "Core/TRPlayerState.h"
#include "Engine/World.h"

void ATRPlayerController::SetLobbyReady(bool bInReady)
{
	ServerSetLobbyReady(bInReady);
}

void ATRPlayerController::RequestStartMatch()
{
	ServerStartMatch();
}

void ATRPlayerController::ServerSetLobbyReady_Implementation(bool bInReady)
{
	if (ATRPlayerState* State = GetPlayerState<ATRPlayerState>())
	{
		State->SetReady(bInReady);
	}
}

void ATRPlayerController::ServerStartMatch_Implementation()
{
	const ATRPlayerState* State = GetPlayerState<ATRPlayerState>();
	if (State == nullptr || !State->IsHost())
	{
		return;
	}

	if (ATRGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATRGameMode>() : nullptr)
	{
		GameMode->StartMatch();
	}
}

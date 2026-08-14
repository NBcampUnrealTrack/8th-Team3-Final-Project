// TRGameMode.cpp


#include "Core/TRGameMode.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Core/TRGameState.h"
#include "Core/TRPlayerState.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Spawning/TRWaveSpawner.h"

void ATRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 리슨 서버에서 로컬 컨트롤러를 가진 사람이 방을 만든 사람이다.
	ATRPlayerState* State = NewPlayer ? NewPlayer->GetPlayerState<ATRPlayerState>() : nullptr;
	if (State != nullptr && NewPlayer->IsLocalController())
	{
		State->SetHost(true);
	}
}

bool ATRGameMode::StartMatch()
{
	ATRGameState* State = GetGameState<ATRGameState>();
	if (State == nullptr || !State->CanStartMatch())
	{
		return false;
	}

	State->SetMatchStarted(true);

	// 레벨에 놓인 스포너를 전부 깨운다. 스포너가 없으면 판만 시작된 상태로 둔다.
	int32 Started = 0;
	for (TActorIterator<ATRWaveSpawner> It(GetWorld()); It; ++It)
	{
		It->StartWaves();
		++Started;
	}

	UE_LOG(LogTRMass, Log, TEXT("판 시작 — 웨이브 스포너 %d개 가동 (준비 %d/%d)"),
		Started, State->GetReadyPlayerCount(), State->GetRequiredReadyCount());

	return true;
}

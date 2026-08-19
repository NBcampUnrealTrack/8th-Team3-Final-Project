// TRGameMode.cpp


#include "Core/TRGameMode.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Core/TRGameState.h"
#include "Core/TRPlayerState.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Spawning/TRMassSpawnerSubsystem.h"
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

bool ATRGameMode::EndMatch()
{
	ATRGameState* State = GetGameState<ATRGameState>();
	if (State == nullptr || State->IsGameOver())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	// 단계를 **먼저** 바꾼다. 아래 정리 과정에서 무엇이 다시 EndMatch 를 부르더라도
	// 위 검사에 걸려 되돌아온다.
	State->SetWavePhase(ETRWavePhase::Over);

	// 1. 새 웨이브가 더 시작되지 않게 한다.
	int32 Stopped = 0;
	for (TActorIterator<ATRWaveSpawner> It(World); It; ++It)
	{
		It->StopWaves();
		++Stopped;
	}

	// 2. 남아 있는 적을 지운다. 스포너만 멈추면 이미 나온 적이 부서진 기차를 계속 때리고,
	//    결과 화면 뒤에서 전투음과 폭발이 이어진다.
	int32 Removed = 0;
	if (UTRMassSpawnerSubsystem* Spawner = World->GetSubsystem<UTRMassSpawnerSubsystem>())
	{
		Removed = Spawner->DespawnChasers(0);
	}

	State->SetAliveEnemyCount(0);

	UE_LOG(LogTRMass, Log, TEXT("판 종료 — 도달 웨이브 %d, 스포너 %d개 정지, 잔존 적 %d마리 정리"),
		State->GetWaveNumber(), Stopped, Removed);

	return true;
}

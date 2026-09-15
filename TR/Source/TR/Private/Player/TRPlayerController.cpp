//TRPlayerController.cpp


#include "Player/TRPlayerController.h"
#include "Core/TRGameState.h"
#include "Debug/TRGameplayLog.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Train/TRInstallablePart.h"
#include "Train/TRTrain.h"
#include "Core/TRGameMode.h"
#include "Core/TRPlayerState.h"

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

void ATRPlayerController::TR_DamageAll(float Damage)
{
#if !UE_BUILD_SHIPPING
	ServerCheatDamageAll(Damage);
#endif
}

void ATRPlayerController::ServerCheatDamageAll_Implementation(float Damage)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!IsValid(World) || Damage <= 0.f)
	{
		return;
	}

	// CurrentHP 를 직접 깎지 않고 TakeDamage 를 태운다 — OnPartDestroyed / OnHullDestroyed 훅과
	// 기존 로그가 실제 피격과 똑같이 돌아야 한다. 이미 파괴된 대상은 각자 TakeDamage 가 걸러낸다.
	int32 PartCount = 0;
	for (TActorIterator<ATRInstallablePart> It(World); It; ++It)
	{
		UGameplayStatics::ApplyDamage(*It, Damage, this, GetPawn(), nullptr);
		++PartCount;
	}

	int32 TrainCount = 0;
	for (TActorIterator<ATRTrain> It(World); It; ++It)
	{
		UGameplayStatics::ApplyDamage(*It, Damage, this, GetPawn(), nullptr);
		++TrainCount;
	}

	UE_LOG(LogTRGameplay, Log, TEXT("Cheat: DamageAll %.0f to %d part(s), %d train(s)"), Damage, PartCount, TrainCount);
#endif
}

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
	// 판이 시작된 뒤에는 준비를 바꿀 수 없다. 판정은 서버가 쥔다.
	const UWorld* World = GetWorld();
	const ATRGameState* GameState = World ? World->GetGameState<ATRGameState>() : nullptr;
	if (!ensureMsgf(GameState, TEXT("서버에 ATRGameState 가 없어 준비 변경을 판정할 수 없습니다.")))
	{
		return;
	}

	if (GameState->IsMatchStarted())
	{
		return;
	}

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

	ATRGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATRGameMode>() : nullptr;
	if (!ensureMsgf(GameMode, TEXT("[ATRPlayerController] 서버에 ATRGameMode 가 없다")))
	{
		return;
	}
	GameMode->StartMatch();
}

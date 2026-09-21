// 핑 발신 테스트 콘솔 명령. 실제 입력(휠)과 같은 경로를 탄다.

#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Ping/BGPingComponent.h"
#include "Ping/BGPingTypes.h"

#if !UE_BUILD_SHIPPING

static void BGPingExec(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (Args.Num() < 2)
	{
		UE_LOG(LogBGPing, Warning, TEXT("[BGPing] 사용법: BG.Ping <좌석|-1> <Attack|Warning|Suspect|Protect|Heal>"));
		return;
	}

	const int32 TargetSeat = FCString::Atoi(*Args[0]);
	const FGameplayTag PingTag = BGPing::ResolvePingTag(Args[1]);
	if (!PingTag.IsValid())
	{
		UE_LOG(LogBGPing, Warning, TEXT("[BGPing] 알 수 없는 핑 종류: %s"), *Args[1]);
		return;
	}

	ABGPlayerController* PC = Cast<ABGPlayerController>(GEngine->GetFirstLocalPlayerController(World));
	UBGPingComponent* PingComp = PC ? PC->GetPingComponent() : nullptr;
	if (!PingComp)
	{
		UE_LOG(LogBGPing, Warning, TEXT("[BGPing] 로컬 PlayerController 에 PingComponent 가 없다"));
		return;
	}

	//	에디터 스크립팅 경로에서 바로 부르면 스크립트 실행 가드가 걸려 멀티캐스트가 로컬로만 돈다.
	//	다음 틱(게임 틱)으로 미뤄 실제 입력과 같은 문맥에서 보낸다.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(PingComp,
		[PingComp, PingTag, TargetSeat]() { PingComp->SendPing(PingTag, TargetSeat); }));
}


static void BGPingNetDiagWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	AGameStateBase* GS = World->GetGameState();
	UNetDriver* Driver = World->GetNetDriver();

	UE_LOG(LogBGPing, Log, TEXT("[NetDiag] World=%s NetMode=%d GS=%s"),
		*World->GetName(), static_cast<int32>(World->GetNetMode()), *GetNameSafe(GS));

	if (GS)
	{
		UE_LOG(LogBGPing, Log, TEXT("[NetDiag]  GS LocalRole=%d RemoteRole=%d Replicated=%d BeingDestroyed=%d NetDriverName=%s TearOff=%d"),
			static_cast<int32>(GS->GetLocalRole()), static_cast<int32>(GS->GetRemoteRole()),
			GS->GetIsReplicated() ? 1 : 0, GS->IsActorBeingDestroyed() ? 1 : 0,
			*GS->GetNetDriverName().ToString(), GS->GetTearOff() ? 1 : 0);
	}

	if (!Driver)
	{
		UE_LOG(LogBGPing, Log, TEXT("[NetDiag]  NetDriver 없음"));
		return;
	}

	UE_LOG(LogBGPing, Log, TEXT("[NetDiag]  Driver=%s IsServer=%d ClientConnections=%d RepDriver=%s"),
		*Driver->NetDriverName.ToString(), Driver->IsServer() ? 1 : 0,
		Driver->ClientConnections.Num(), *GetNameSafe(Driver->GetReplicationDriver()));

	for (int32 i = 0; i < Driver->ClientConnections.Num(); ++i)
	{
		UNetConnection* Conn = Driver->ClientConnections[i];
		if (!Conn)
		{
			continue;
		}

		const UActorChannel* Channel = GS ? Conn->FindActorChannelRef(GS) : nullptr;
		const bool bLevelReady = (GS && GS->GetLevel()) ? Conn->ClientHasInitializedLevel(GS->GetLevel()) : false;

		UE_LOG(LogBGPing, Log, TEXT("[NetDiag]  Conn[%d] State=%d PC=%s ViewTarget=%s GSChannel=%s LevelReady=%d"),
			i, static_cast<int32>(Conn->GetConnectionState()), *GetNameSafe(Conn->PlayerController),
			*GetNameSafe(Conn->ViewTarget), Channel ? TEXT("있음") : TEXT("없음"), bLevelReady ? 1 : 0);
	}
}

static void BGPingNetDiagExec(const TArray<FString>& Args, UWorld* World)
{
	//	실행한 월드뿐 아니라 이 프로세스의 모든 게임 월드를 찍는다(PIE 클라 포함).
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World() && Context.World()->IsGameWorld())
		{
			BGPingNetDiagWorld(Context.World());
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBGPingNetDiagCmd(
	TEXT("BG.PingNetDiag"),
	TEXT("핑 멀티캐스트가 왜 안 나가는지 네트워크 상태를 찍는다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BGPingNetDiagExec));

static void BGPingWheelExec(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	const int32 TargetSeat = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : INDEX_NONE;
	ABGPlayerController* PC = Cast<ABGPlayerController>(GEngine->GetFirstLocalPlayerController(World));
	UBGPingComponent* PingComp = PC ? PC->GetPingComponent() : nullptr;
	if (!PingComp)
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(PingComp,
		[PingComp, TargetSeat]() { PingComp->DebugOpenWheel(TargetSeat); }));
}

static FAutoConsoleCommandWithWorldAndArgs GBGPingWheelCmd(
	TEXT("BG.PingWheel"),
	TEXT("휠을 열어 본다. BG.PingWheel <대상 좌석|-1>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BGPingWheelExec));

static FAutoConsoleCommandWithWorldAndArgs GBGPingCmd(
	TEXT("BG.Ping"),
	TEXT("핑을 보낸다. BG.Ping <좌석|-1> <Attack|Warning|Suspect|Protect|Heal>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BGPingExec));

#endif

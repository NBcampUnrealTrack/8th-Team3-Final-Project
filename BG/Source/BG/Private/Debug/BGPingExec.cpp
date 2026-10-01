// 핑 발신 테스트 콘솔 명령. 실제 입력(휠)과 같은 경로를 탄다.

#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Core/BGPlayerState.h"
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

namespace
{
	//	지연 콜백마다 다시 찾는다 — PIE 종료·트래블 뒤의 뷰포트·월드를 붙잡지 않는다.
	FViewport* FindClickViewport(const TWeakObjectPtr<ABGPlayerController>& WeakPC, const TWeakObjectPtr<UWorld>& WeakWorld)
	{
		ABGPlayerController* PC = WeakPC.Get();
		if (!PC || !WeakWorld.IsValid() || PC->GetWorld() != WeakWorld.Get())
		{
			return nullptr;
		}
		ULocalPlayer* LP = PC->GetLocalPlayer();
		return (LP && LP->ViewportClient) ? LP->ViewportClient->Viewport : nullptr;
	}

	void SimulateClick(ABGPlayerController* PC)
	{
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.f));
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.f));
	}
}

static void BGPingClickExec(const TArray<FString>& Args, UWorld* World)
{
	if (!World || Args.Num() < 1)
	{
		UE_LOG(LogBGPing, Warning, TEXT("[BGPing] 사용법: BG.PingClick <좌석> [dx dy] — 좌석 캐릭터 위로 커서를 옮겨 실제 좌클릭을 넣는다"));
		return;
	}

	const int32 Seat = FCString::Atoi(*Args[0]);
	const float Dx = Args.Num() > 2 ? FCString::Atof(*Args[1]) : 0.f;
	const float Dy = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.f;

	ABGPlayerController* PC = Cast<ABGPlayerController>(GEngine->GetFirstLocalPlayerController(World));
	ABGGameState* GS = World->GetGameState<ABGGameState>();
	ABGPlayerState* PS = GS ? GS->GetPlayerStateBySeat(Seat) : nullptr;
	APawn* Target = PS ? PS->GetPawn() : nullptr;
	if (!PC || !Target)
	{
		UE_LOG(LogBGPing, Warning, TEXT("[BGPing] PingClick — 컨트롤러 또는 좌석 %d 캐릭터 없음"), Seat);
		return;
	}

	TWeakObjectPtr<ABGPlayerController> WeakPC(PC);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	TWeakObjectPtr<APawn> WeakTarget(Target);

	//	다음 틱에 커서를 옮기고, 호버 판정이 갱신될 시간을 둔 뒤 눌렀다 뗀다.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(PC, [WeakPC, WeakWorld, WeakTarget, Dx, Dy, Seat]()
	{
		ABGPlayerController* P = WeakPC.Get();
		APawn* T = WeakTarget.Get();
		FViewport* Viewport = FindClickViewport(WeakPC, WeakWorld);
		FVector2D Screen;
		if (!Viewport || !T || !P->ProjectWorldLocationToScreen(T->GetActorLocation() + FVector(0.f, 0.f, 60.f), Screen))
		{
			return;
		}

		Viewport->SetMouse(FMath::RoundToInt(Screen.X), FMath::RoundToInt(Screen.Y));
		UE_LOG(LogBGPing, Log, TEXT("[BGPing] PingClick — 좌석 %d 커서 (%.0f, %.0f)"), Seat, Screen.X, Screen.Y);

		FTimerHandle PressHandle;
		P->GetWorldTimerManager().SetTimer(PressHandle, FTimerDelegate::CreateWeakLambda(P, [WeakPC, WeakWorld, Screen, Dx, Dy]()
		{
			ABGPlayerController* P2 = WeakPC.Get();
			if (!P2 || !FindClickViewport(WeakPC, WeakWorld))
			{
				return;
			}
			SimulateClick(P2);

			//	휠이 열렸으면 끌 방향으로 커서를 옮긴 뒤 한 번 더 눌러 확정한다.
			FTimerHandle ConfirmHandle;
			P2->GetWorldTimerManager().SetTimer(ConfirmHandle, FTimerDelegate::CreateWeakLambda(P2, [WeakPC, WeakWorld, Screen, Dx, Dy]()
			{
				ABGPlayerController* P3 = WeakPC.Get();
				FViewport* V = FindClickViewport(WeakPC, WeakWorld);
				if (!P3 || !V)
				{
					return;
				}
				V->SetMouse(FMath::RoundToInt(Screen.X + Dx), FMath::RoundToInt(Screen.Y + Dy));
				SimulateClick(P3);
			}), 0.4f, false);
		}), 0.3f, false);
	}));
}

static void BGPingMouseExec(const TArray<FString>& Args, UWorld* World)
{
	if (!World || Args.Num() < 2)
	{
		UE_LOG(LogBGPing, Warning, TEXT("[BGPing] 사용법: BG.PingMouse <x> <y> [abs] — 뷰포트 커서를 상대(기본)·절대 좌표로 옮긴다"));
		return;
	}

	ABGPlayerController* PC = Cast<ABGPlayerController>(GEngine->GetFirstLocalPlayerController(World));
	FViewport* Viewport = FindClickViewport(PC, World);
	if (!Viewport)
	{
		return;
	}

	const bool bAbsolute = Args.Num() > 2 && Args[2] == TEXT("abs");
	const FIntPoint Now(Viewport->GetMouseX(), Viewport->GetMouseY());
	const int32 X = FCString::Atoi(*Args[0]) + (bAbsolute ? 0 : Now.X);
	const int32 Y = FCString::Atoi(*Args[1]) + (bAbsolute ? 0 : Now.Y);
	Viewport->SetMouse(X, Y);
	UE_LOG(LogBGPing, Log, TEXT("[BGPing] PingMouse — (%d, %d) → (%d, %d)"), Now.X, Now.Y, X, Y);
}

static FAutoConsoleCommandWithWorldAndArgs GBGPingMouseCmd(
	TEXT("BG.PingMouse"),
	TEXT("뷰포트 커서를 옮긴다(휠 하이라이트 캡처용). BG.PingMouse <x> <y> [abs]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BGPingMouseExec));

static FAutoConsoleCommandWithWorldAndArgs GBGPingClickCmd(
	TEXT("BG.PingClick"),
	TEXT("좌석 캐릭터 위를 실제 좌클릭 입력으로 누른다. BG.PingClick <좌석> [dx dy]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BGPingClickExec));

static FAutoConsoleCommandWithWorldAndArgs GBGPingCmd(
	TEXT("BG.Ping"),
	TEXT("핑을 보낸다. BG.Ping <좌석|-1> <Attack|Warning|Suspect|Protect|Heal>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BGPingExec));

#endif

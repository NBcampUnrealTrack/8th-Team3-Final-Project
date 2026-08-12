#include "Lobby/BGRopeComponent.h"

#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "Debug/BGCardLog.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UBGRopeComponent::UBGRopeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UBGRopeComponent* UBGRopeComponent::Find(const UWorld* World)
{
	//	GameState 는 클라에도 있다 — 남은 시간 표시가 서버 전용이면 안 된다.
	ABGGameState* GS = World ? World->GetGameState<ABGGameState>() : nullptr;
	return GS ? GS->FindComponentByClass<UBGRopeComponent>() : nullptr;
}

void UBGRopeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBGRopeComponent, Rope);
}

void UBGRopeComponent::OnRep_Rope()
{
	BroadcastRope();
}

void UBGRopeComponent::BroadcastRope()
{
	//	멈춤 상태부터 알린다. 남은 시간을 받은 UI 가 곧바로 흐르는 것으로 그리지 않게 한다.
	if (BroadcastedPaused != Rope.bPaused)
	{
		BroadcastedPaused = Rope.bPaused;
		OnRopePausedChanged.Broadcast(Rope.bPaused);
	}

	OnRopeChanged.Broadcast(Rope.Seat, GetSecondsRemaining(), Rope.Length);
}

ABGGameState* UBGRopeComponent::GetGameState() const
{
	return Cast<ABGGameState>(GetOwner());
}

void UBGRopeComponent::BeginRope(int32 Seat)
{
	ABGGameState* GS = GetGameState();
	if (!GS || !GS->HasAuthority())
	{
		return;
	}

	StopRope();

	if (Seat == INDEX_NONE || !GS->IsMatchRunning())
	{
		return;
	}

	//	자리를 비운 사람은 짧게 — 어차피 아무 입력도 오지 않는다.
	const ABGPlayerState* PS = GS->GetPlayerStateBySeat(Seat);
	const bool bAbsent = (PS == nullptr) || PS->IsDisconnected();

	Rope.Length          = FMath::Max(0.1f, bAbsent ? AbsentTurnSeconds : TurnSeconds);
	Rope.Seat            = Seat;
	Rope.EndServerTime   = GS->GetServerWorldTimeSeconds() + Rope.Length;
	Rope.bPaused         = false;

	//	멈춘 채로 붙는 경우를 위해 전체 길이를 미리 넣어 둔다.
	Rope.PausedRemaining = Rope.Length;

	UE_LOG(LogBGCard, Log, TEXT("[Rope] 좌석 %d 밧줄 점화 — %.0f초%s"),
		Seat, Rope.Length, bAbsent ? TEXT(" (이탈 중)") : TEXT(""));

	//	연출 중에 턴이 넘어가면 새 밧줄도 멈춘 채로 시작해야 한다.
	//	타이머를 거는 것도 여기 안에서 한다 — 멈춘 상태면 걸지 않는다.
	ApplyPauseState();

	BroadcastRope();
}

void UBGRopeComponent::StopRope()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RopeTimer);
	}

	if (Rope.Seat == INDEX_NONE)
	{
		return;   //	이미 꺼져 있다. 같은 통지를 두 번 보내지 않는다.
	}

	//	잡고 있던 이유(PauseHolders)는 그대로 둔다. 밧줄이 아니라 상황을 잡고 있는 것이라,
	//	턴이 넘어가 새로 붙는 밧줄도 같은 이유로 멈춰 있어야 한다.
	Rope = FBGRopeState();
	Rope.bPaused = (PauseHolders.Num() > 0);

	BroadcastRope();
}

void UBGRopeComponent::PauseRope(FName Reason)
{
	const ABGGameState* GS = GetGameState();
	if (!GS || !GS->HasAuthority() || Reason.IsNone())
	{
		return;
	}

	if (PauseHolders.Contains(Reason))
	{
		return;   //	같은 이유로 두 번 잡아도 하나로 친다.
	}

	PauseHolders.Add(Reason);
	UE_LOG(LogBGCard, Log, TEXT("[Rope] 멈춤 — %s (잡고 있는 이유 %d개)"),
		*Reason.ToString(), PauseHolders.Num());

	ApplyPauseState();
	BroadcastRope();
}

void UBGRopeComponent::ResumeRope(FName Reason)
{
	const ABGGameState* GS = GetGameState();
	if (!GS || !GS->HasAuthority())
	{
		return;
	}

	if (PauseHolders.Remove(Reason) == 0)
	{
		return;   //	잡은 적이 없다. 짝이 안 맞는 호출은 조용히 무시한다.
	}

	UE_LOG(LogBGCard, Log, TEXT("[Rope] 놓음 — %s (남은 이유 %d개)"),
		*Reason.ToString(), PauseHolders.Num());

	ApplyPauseState();
	BroadcastRope();
}

void UBGRopeComponent::ClearRopePauses()
{
	const ABGGameState* GS = GetGameState();
	if (!GS || !GS->HasAuthority() || PauseHolders.IsEmpty())
	{
		return;
	}

	PauseHolders.Reset();
	ApplyPauseState();
	BroadcastRope();
}

void UBGRopeComponent::ApplyPauseState()
{
	UWorld* World = GetWorld();
	const ABGGameState* GS = GetGameState();
	if (!World || !GS)
	{
		return;
	}

	//	남은 시간을 다시 잴지는 직전 상태로 정한다.
	const bool bWasPaused = Rope.bPaused;
	const bool bShouldPause = (PauseHolders.Num() > 0);
	Rope.bPaused = bShouldPause;

	//	감시 타이머는 밧줄이 붙어 있든 없든 건다. 잡아 놓고 안 놓는 쪽을 잡으려는 것이다.
	World->GetTimerManager().ClearTimer(PauseWatchdog);
	if (bShouldPause && PauseTimeoutSeconds > 0.f)
	{
		World->GetTimerManager().SetTimer(
			PauseWatchdog, this, &UBGRopeComponent::HandlePauseTimeout, PauseTimeoutSeconds, /*bLoop=*/false);
	}

	if (Rope.Seat == INDEX_NONE)
	{
		return;   //	멈출 불이 없다. 이유만 기억해 두고 다음 BeginRope 가 멈춘 채 시작한다.
	}

	if (bShouldPause)
	{
		//	이미 멈춰 있으면 다시 재지 않는다. 타이머가 없어 전체 길이로 되돌아간다.
		if (bWasPaused)
		{
			return;
		}

		//	남은 시간은 타이머에게 직접 묻는다. EndServerTime 은 복제용 시계라 실제보다
		//	뒤처져 있어서, 그걸로 계산하면 멈출 때마다 시간이 조금씩 늘어난다.
		const float Remaining = World->GetTimerManager().GetTimerRemaining(RopeTimer);
		if (Remaining > 0.f)
		{
			Rope.PausedRemaining = Remaining;
		}
		//	타이머가 없으면 방금 붙은 밧줄이다 — BeginRope 가 넣어 둔 전체 길이를 그대로 둔다.

		World->GetTimerManager().ClearTimer(RopeTimer);
		return;
	}

	//	다시 태운다. 멈춰 있던 만큼 끝나는 시각을 뒤로 민다.
	const float Remaining = (Rope.PausedRemaining > 0.f) ? Rope.PausedRemaining : Rope.Length;
	Rope.EndServerTime   = GS->GetServerWorldTimeSeconds() + Remaining;
	Rope.PausedRemaining = 0.f;

	World->GetTimerManager().SetTimer(
		RopeTimer, this, &UBGRopeComponent::HandleRopeBurnedOut, FMath::Max(0.01f, Remaining), /*bLoop=*/false);
}

void UBGRopeComponent::HandlePauseTimeout()
{
	//	놓아 주는 쪽이 빠졌다. 밧줄은 판이 멈추는 것을 막는 장치이므로 스스로 풀고 계속 간다.
	UE_LOG(LogBGCard, Warning, TEXT("[Rope] %.0f초 넘게 멈춰 있다 — 잡고 있던 이유 %d개를 강제로 놓는다"),
		PauseTimeoutSeconds, PauseHolders.Num());

	ClearRopePauses();
}

float UBGRopeComponent::GetSecondsRemaining() const
{
	const ABGGameState* GS = GetGameState();
	if (!GS || Rope.Seat == INDEX_NONE)
	{
		return 0.f;
	}

	//	멈춰 있는 동안 EndServerTime 은 지나간 값이라 그대로 쓰면 0 으로 보인다.
	if (Rope.bPaused)
	{
		return FMath::Max(0.f, Rope.PausedRemaining);
	}

	return FMath::Max(0.f, Rope.EndServerTime - GS->GetServerWorldTimeSeconds());
}

float UBGRopeComponent::GetRemainingRatio() const
{
	return (Rope.Length > 0.f) ? FMath::Clamp(GetSecondsRemaining() / Rope.Length, 0.f, 1.f) : 0.f;
}

void UBGRopeComponent::HandleRopeBurnedOut()
{
	ABGGameState* GS = GetGameState();
	if (!GS || !GS->HasAuthority())
	{
		return;
	}

	//	멈춰 있는데 타이머가 살아 있었다 = 순서가 꼬였다. 턴을 넘기지 않고 상태만 다시 맞춘다.
	if (Rope.bPaused)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Rope] 멈춘 밧줄이 소진 통지를 보냈다 — 무시하고 상태를 다시 맞춘다"));
		ApplyPauseState();
		return;
	}

	const int32 Seat = Rope.Seat;
	StopRope();   //	아래에서 턴이 넘어가며 새 밧줄이 붙는다. 먼저 꺼 두어야 겹치지 않는다.

	ABGPlayerState* PS = GS->GetPlayerStateBySeat(Seat);
	if (!PS)
	{
		//	좌석 주인이 아예 사라졌다(예약 실패 등). 판을 세우지 않는 것이 우선이다.
		UE_LOG(LogBGCard, Warning, TEXT("[Rope] 좌석 %d 의 주인이 없다 — 턴만 넘긴다"), Seat);
		GS->ForceEndTurn(Seat, TEXT("밧줄 소진 (좌석 주인 없음)"));
		return;
	}

	const bool  bAbsent = PS->IsDisconnected();
	const int32 Strikes = PS->AddRopeStrike();

	//	턴 시간 초과는 정상 진행의 일부다 — 이탈자만 사망으로 이어지므로 그때만 눈에 띄게 남긴다.
	UE_LOG(LogBGCard, Log, TEXT("[Rope] 좌석 %d 밧줄 소진 — 누적 %d/%d%s"),
		Seat, Strikes, MaxStrikes, bAbsent ? TEXT(" (이탈 중)") : TEXT(""));

	//	자리를 비운 채 정해진 횟수를 넘겼다 = 돌아오지 않는다.
	//	접속 중인 사람은 몇 번을 태워도 죽이지 않는다 — 잠수 처벌은 다른 문제다.
	if (bAbsent && Strikes >= MaxStrikes)
	{
		GS->KillSeat(Seat, TEXT("이탈 상태로 밧줄 소진 — 복귀 포기"));
		return;   //	사망 처리가 승패 판정과 다음 턴까지 이어 간다.
	}

	GS->ForceEndTurn(Seat, TEXT("밧줄 소진"));
}

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

	Rope.Length        = FMath::Max(0.1f, bAbsent ? AbsentTurnSeconds : TurnSeconds);
	Rope.Seat          = Seat;
	Rope.EndServerTime = GS->GetServerWorldTimeSeconds() + Rope.Length;

	GetWorld()->GetTimerManager().SetTimer(
		RopeTimer, this, &UBGRopeComponent::HandleRopeBurnedOut, Rope.Length, /*bLoop=*/false);

	UE_LOG(LogBGCard, Log, TEXT("[Rope] 좌석 %d 밧줄 점화 — %.0f초%s"),
		Seat, Rope.Length, bAbsent ? TEXT(" (이탈 중)") : TEXT(""));

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

	Rope = FBGRopeState();
	BroadcastRope();
}

float UBGRopeComponent::GetSecondsRemaining() const
{
	const ABGGameState* GS = GetGameState();
	if (!GS || Rope.Seat == INDEX_NONE)
	{
		return 0.f;
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

	//	연출 중에는 턴을 못 넘긴다(RequestEndTurn 과 같은 이유). 연출이 끝나면 다시 잰다.
	if (GS->IsCinematicPlaying())
	{
		UE_LOG(LogBGCard, Log, TEXT("[Rope] 연출 재생 중 — 밧줄을 다시 붙인다"));
		BeginRope(Rope.Seat);
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

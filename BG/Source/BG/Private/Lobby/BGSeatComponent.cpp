#include "Lobby/BGSeatComponent.h"

#include "Core/BGCampfire.h"      // 좌석 "위치" 는 캠프파이어가 계산한다
#include "Core/BGGameMode.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "Debug/BGNetLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "Math/UnrealMathUtility.h"

const TCHAR* UBGSeatComponent::SeatStartTagPrefix = TEXT("Seat");

UBGSeatComponent::UBGSeatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UBGSeatComponent* UBGSeatComponent::Find(const UWorld* World)
{
	// GetAuthGameMode 는 서버에서만 유효하다 — 클라에서는 자연히 nullptr.
	ABGGameMode* GM = World ? World->GetAuthGameMode<ABGGameMode>() : nullptr;
	return GM ? GM->FindComponentByClass<UBGSeatComponent>() : nullptr;
}

ABGGameMode* UBGSeatComponent::GetGameMode() const
{
	return Cast<ABGGameMode>(GetOwner());
}

void UBGSeatComponent::CollectPlayerStates(TArray<ABGPlayerState*>& OutStates) const
{
	OutStates.Reset();

	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	for (APlayerState* Base : GS->PlayerArray)
	{
		if (ABGPlayerState* PS = Cast<ABGPlayerState>(Base))
		{
			OutStates.Add(PS);
		}
	}
}

void UBGSeatComponent::BuildSeatOrder(TArray<int32>& OutSeats)
{
	OutSeats.Reset();
	OutSeats.Reserve(ABGGameMode::MaxPlayers);

	// 짝수 자리를 먼저, 그다음 홀수 — 인원이 적어도 테이블에 고르게 앉는다.
	for (int32 Parity = 0; Parity < 2; ++Parity)
	{
		for (int32 Seat = Parity; Seat < ABGGameMode::MaxPlayers; Seat += 2)
		{
			OutSeats.Add(Seat);
		}
	}
}

bool UBGSeatComponent::IsSeatTaken(int32 Seat) const
{
	if (Seat == INDEX_NONE)
	{
		return false;
	}

	TArray<ABGPlayerState*> States;
	CollectPlayerStates(States);

	for (const ABGPlayerState* PS : States)
	{
		if (PS->GetSeatIndex() == Seat)
		{
			return true;
		}
	}
	return false;
}

AActor* UBGSeatComponent::FindSeatStart(int32 Seat) const
{
	UWorld* World = GetWorld();
	if (Seat == INDEX_NONE || !World)
	{
		return nullptr;
	}

	const FName WantTag(*FString::Printf(TEXT("%s%d"), SeatStartTagPrefix, Seat));
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (It->PlayerStartTag == WantTag)
		{
			return *It;
		}
	}
	return nullptr;
}

AActor* UBGSeatComponent::ChooseStartFor(const AController* Player) const
{
	const ABGPlayerState* PS = Player ? Player->GetPlayerState<ABGPlayerState>() : nullptr;
	if (AActor* Start = PS ? FindSeatStart(PS->GetSeatIndex()) : nullptr)
	{
		return Start;
	}

	// 폴백은 임의 지점이라 자리가 어긋난다 — 조용히 넘어가면 원인 찾기가 어렵다.
	UE_LOG(LogBGNet, Warning, TEXT("[Seat] 자리 선택 실패 — 좌석 %d, 엔진 폴백(%s)"),
		PS ? PS->GetSeatIndex() : INDEX_NONE, *GetNameSafe(Player));
	return nullptr;
}

void UBGSeatComponent::SnapPawnToSeat(ABGPlayerState* PS) const
{
	APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	FVector Loc;
	FRotator Rot;

	//	캠프파이어가 있으면 그쪽이 우선이다. 자리 간격이 접속 인원수로 결정되므로
	//	레벨에 고정 배치한 PlayerStart 로는 4인과 7인을 같이 만족시킬 수 없다.
	//	⚠️ 두 경로가 모두 폰을 옮기면 나중에 부른 쪽이 이겨 자리가 튄다. 여기서 하나로 고른다.
	FTransform SeatTransform;
	const ABGCampfire* Campfire = ABGCampfire::Find(GetWorld());

	if (Campfire && Campfire->GetTransformForSeat(PS->GetSeatIndex(), SeatTransform))
	{
		Loc = SeatTransform.GetLocation();
		Rot = SeatTransform.GetRotation().Rotator();
	}
	else if (const AActor* Start = FindSeatStart(PS->GetSeatIndex()))
	{
		//	캠프파이어가 없는 레벨(테스트맵 등)에서는 기존 PlayerStart 방식을 그대로 쓴다.
		Loc = Start->GetActorLocation();
		Rot = Start->GetActorRotation();
	}
	else
	{
		return;
	}

	// TeleportTo 는 겹침을 밀어내 준다 — SetActorLocation 은 자리에 끼일 수 있다.
	Pawn->TeleportTo(Loc, Rot);

	// 컨트롤 회전까지 맞춰야 시선이 테이블 중앙을 본다.
	if (AController* C = Pawn->GetController())
	{
		C->SetControlRotation(Rot);
	}
}

int32 UBGSeatComponent::AssignSeatOnJoin(APlayerController* NewPlayer)
{
	const ABGGameMode* GM = GetGameMode();
	if (!GM || !GM->HasAuthority())
	{
		return INDEX_NONE;
	}

	ABGPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ABGPlayerState>() : nullptr;
	if (!PS)
	{
		UE_LOG(LogBGNet, Warning, TEXT("[Seat] 입장 배정 실패 — PlayerState 없음(%s)."),
			NewPlayer ? *NewPlayer->GetName() : TEXT("null"));
		return INDEX_NONE;
	}

	// 재입장/중복 호출로 이미 자리가 있으면 그대로 둔다.
	if (PS->GetSeatIndex() != INDEX_NONE)
	{
		return PS->GetSeatIndex();
	}

	TArray<int32> Order;
	BuildSeatOrder(Order);

	for (const int32 Seat : Order)
	{
		if (!IsSeatTaken(Seat))
		{
			PS->SetSeatIndex(Seat);
			// 이 시점엔 플레이어 이름이 아직 비어 있을 수 있어 컨트롤러 이름으로 대체한다.
			UE_LOG(LogBGNet, Log, TEXT("[Seat] 입장 배정 — %s → 좌석 %d"),
				PS->GetPlayerName().IsEmpty() ? *GetNameSafe(NewPlayer) : *PS->GetPlayerName(), Seat);
			return Seat;
		}
	}

	UE_LOG(LogBGNet, Warning, TEXT("[Seat] 빈 좌석 없음(정원 %d) — %s 미배정."),
		ABGGameMode::MaxPlayers, *PS->GetPlayerName());
	return INDEX_NONE;
}

void UBGSeatComponent::ReleaseSeat(AController* Exiting)
{
	const ABGGameMode* GM = GetGameMode();
	if (!GM || !GM->HasAuthority())
	{
		return;
	}

	ABGPlayerState* PS = Exiting ? Exiting->GetPlayerState<ABGPlayerState>() : nullptr;
	if (!PS || PS->GetSeatIndex() == INDEX_NONE)
	{
		return;
	}

	UE_LOG(LogBGNet, Log, TEXT("[Seat] 좌석 반납 — %s (좌석 %d)"),
		*PS->GetPlayerName(), PS->GetSeatIndex());
	PS->SetSeatIndex(INDEX_NONE);
}

void UBGSeatComponent::RandomizeSeats()
{
	const ABGGameMode* GM = GetGameMode();
	if (!GM || !GM->HasAuthority())
	{
		return;
	}

	TArray<ABGPlayerState*> States;
	CollectPlayerStates(States);

	const int32 Num = States.Num();
	if (Num == 0)
	{
		UE_LOG(LogBGNet, Warning, TEXT("[Seat] 랜덤 배정: 대상 플레이어가 없습니다."));
		return;
	}

	// 인원수만큼의 자리를 입장 배정과 같은 규칙(0,2,4,6,...)으로 잡는다 —
	// 4명이 0~3 에 몰려 앉지 않고 테이블에 흩어진다.
	TArray<int32> Seats;
	BuildSeatOrder(Seats);
	if (Num > Seats.Num())
	{
		UE_LOG(LogBGNet, Warning, TEXT("[Seat] 인원 %d 명이 정원 %d 을 넘어 초과분은 미배정."),
			Num, Seats.Num());
	}
	const int32 Take = FMath::Min(Num, Seats.Num());
	Seats.SetNum(Take);

	// 자리 집합은 고정하고 사람만 Fisher-Yates 로 섞는다.
	for (int32 i = Take - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Seats.Swap(i, j);
	}

	for (int32 i = 0; i < Take; ++i)
	{
		States[i]->SetSeatIndex(Seats[i]);

		// 좌석이 바뀌었으니 폰도 그 자리로 옮긴다(참여 순으로 앉아 있던 상태를 갱신).
		SnapPawnToSeat(States[i]);

		UE_LOG(LogBGNet, Log, TEXT("[Seat] 랜덤 배정 — %s → 좌석 %d%s"),
			*States[i]->GetPlayerName(), Seats[i],
			FindSeatStart(Seats[i]) ? TEXT("") : TEXT(" (자리 액터 없음)"));
	}
}

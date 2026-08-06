#include "Lobby/BGSeatComponent.h"

#include "Core/BGCampfire.h"      // 좌석 "위치" 는 캠프파이어가 계산한다
#include "Core/BGGameInstance.h"   // PlayerKeyOption
#include "Core/BGGameMode.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "Lobby/BGRopeComponent.h"
#include "Debug/BGNetLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
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

	// PS_Seat* 가 없어도 문제 없다 — 실제 스폰 위치는 캠프파이어가 계산한다
	// (ABGGameMode::SpawnDefaultPawnFor). 여기 반환값은 그 계산이 불가능할 때의 폴백이다.
	return nullptr;
}

void UBGSeatComponent::SnapPawnToSeat(ABGPlayerState* PS) const
{
	APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	//	캠프파이어가 있으면 그쪽이 우선이다. 자리 간격이 접속 인원수로 결정되므로
	//	레벨에 고정 배치한 PlayerStart 로는 4인과 7인을 같이 만족시킬 수 없다.
	//	주의: 두 경로가 모두 폰을 옮기면 나중에 부른 쪽이 이겨 자리가 튄다. 여기서 하나로 고른다.
	FTransform SeatTransform;
	const ABGCampfire* Campfire = ABGCampfire::Find(GetWorld());

	if (Campfire && Campfire->GetTransformForSeat(PS->GetSeatIndex(), SeatTransform))
	{
		//	캡슐 반높이 보정과 실패 로그가 PlacePawn 안에 있다 — 여기서 직접 TeleportTo 하지 말 것.
		Campfire->PlacePawn(Pawn, SeatTransform);
		return;
	}

	//	캠프파이어가 없는 레벨(테스트맵 등)에서는 기존 PlayerStart 방식을 그대로 쓴다.
	//	PlayerStart 는 캡슐이 지면 위에 서도록 배치되어 있으므로 높이 보정이 필요 없다.
	const AActor* Start = FindSeatStart(PS->GetSeatIndex());
	if (!Start)
	{
		return;
	}

	const FVector  Loc = Start->GetActorLocation();
	const FRotator Rot = Start->GetActorRotation();

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

// ══════════════════════════════════════════════════════════════════════════════════════
//  이탈 좌석 예약
// ══════════════════════════════════════════════════════════════════════════════════════

FString UBGSeatComponent::MakeNetIdKey(const FUniqueNetIdRepl& UniqueId)
{
	//	FUniqueNetIdRepl::ToString 은 이 모듈에서 링크되지 않아 안쪽 id 로 가상 호출한다.
	const FUniqueNetIdPtr Id = UniqueId.GetUniqueNetId();
	return Id.IsValid() ? Id->ToString() : FString();
}

FString UBGSeatComponent::ParsePlayerKey(const FString& Options)
{
	return UGameplayStatics::ParseOption(Options, UBGGameInstance::PlayerKeyOption);
}

const UBGSeatComponent::FBGSeatReservation* UBGSeatComponent::FindReservation(
	const FString& PlayerKey, const FString& NetId) const
{
	//	1순위 — 클라가 들고 다니는 열쇠. 다시 접속해도 같은 값이라 이것만으로 끝난다.
	if (!PlayerKey.IsEmpty())
	{
		if (const FBGSeatReservation* ByKey = Reservations.FindByPredicate(
				[&PlayerKey](const FBGSeatReservation& R) { return R.PlayerKey == PlayerKey; }))
		{
			return ByKey;
		}
	}

	//	2순위 — 계정 식별자. 열쇠를 안 보내는 클라나 EOS/Steam 환경을 위한 경로다.
	if (!NetId.IsEmpty())
	{
		return Reservations.FindByPredicate(
			[&NetId](const FBGSeatReservation& R) { return !R.NetId.IsEmpty() && R.NetId == NetId; });
	}

	return nullptr;
}

bool UBGSeatComponent::HasReservedSeat(const FString& PlayerKey, const FUniqueNetIdRepl& UniqueId) const
{
	return FindReservation(PlayerKey, MakeNetIdKey(UniqueId)) != nullptr;
}

void UBGSeatComponent::ClearReservation(int32 Seat)
{
	Reservations.RemoveAll([Seat](const FBGSeatReservation& R) { return R.Seat == Seat; });
}

bool UBGSeatComponent::ReserveSeatOnLeave(AController* Exiting)
{
	const ABGGameMode* GM = GetGameMode();
	if (!GM || !GM->HasAuthority())
	{
		return false;
	}

	//	판이 돌지 않을 때는 예약할 이유가 없다 — 로비에서는 그냥 자리를 비우면 된다.
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS || !GS->IsMatchRunning())
	{
		return false;
	}

	ABGPlayerState* PS = Exiting ? Exiting->GetPlayerState<ABGPlayerState>() : nullptr;
	if (!PS || PS->GetSeatIndex() == INDEX_NONE || PS->IsDead())
	{
		//	죽은 사람은 기다릴 이유가 없다 — 이미 판에서 빠진 자리다.
		return false;
	}

	//	같은 좌석의 옛 예약은 지우고 새로 쓴다.
	ClearReservation(PS->GetSeatIndex());

	FBGSeatReservation& Res = Reservations.AddDefaulted_GetRef();
	Res.PlayerKey = PS->GetPlayerKey();
	Res.NetId     = MakeNetIdKey(PS->GetUniqueId());
	Res.Seat      = PS->GetSeatIndex();
	Res.State     = PS;
	//	폰을 월드에 남기는 일은 ABGPlayerController::Destroyed 가 이미 했다 —
	//	Logout 시점에는 폰이 떨어져 나간 뒤라 여기서는 잡을 수 없다.
	Res.Pawn      = PS->GetAbandonedPawn();

	if (Res.PlayerKey.IsEmpty() && Res.NetId.IsEmpty())
	{
		//	식별할 방법이 없으면 돌아와도 못 알아본다. 자리는 그대로 지킨다(승패 판정 때문).
		UE_LOG(LogBGNet, Warning, TEXT("[Seat] 좌석 %d 이탈 — 식별자 없음, 재접속 매칭 불가"),
			Res.Seat);
	}

	PS->SetDisconnected(true);

	UE_LOG(LogBGNet, Log, TEXT("[Seat] 좌석 %d 이탈 — 자리를 비워 두고 복귀 대기 (%s)"),
		Res.Seat, *PS->GetPlayerName());

	//	자기 턴에 나갔다면 지금 밧줄은 접속자용 긴 것이다. 아무도 입력하지 않을 자리를
	//	그만큼 기다릴 이유가 없어 짧은 것으로 갈아 붙인다.
	if (GS->GetCurrentSeat() == Res.Seat)
	{
		if (UBGRopeComponent* Rope = GS->GetRope())
		{
			UE_LOG(LogBGNet, Log, TEXT("[Seat] 좌석 %d 는 진행 중인 턴 — 밧줄을 짧게 다시 붙인다"),
				Res.Seat);
			Rope->BeginRope(Res.Seat);
		}
	}

	return true;
}

bool UBGSeatComponent::RestoreSeatOnRejoin(APlayerController* Rejoining, const FString& PlayerKey,
	const FUniqueNetIdRepl& UniqueId)
{
	const ABGGameMode* GM = GetGameMode();
	if (!GM || !GM->HasAuthority() || !Rejoining)
	{
		return false;
	}

	const FBGSeatReservation* Found = FindReservation(PlayerKey, MakeNetIdKey(UniqueId));
	if (!Found)
	{
		return false;
	}

	//	사본으로 받아 둔다 — 아래에서 배열을 건드리면 포인터가 무효가 된다.
	const FBGSeatReservation Res = *Found;
	const int32 ResSeat = Res.Seat;

	ABGPlayerState* OldPS = Res.State.Get();
	ABGPlayerState* NewPS = Rejoining->GetPlayerState<ABGPlayerState>();
	if (!NewPS)
	{
		return false;
	}

	if (!OldPS || OldPS == NewPS)
	{
		//	살려 둔 상태가 사라졌다(판 종료 등). 예약만 정리하고 평소 경로로 보낸다.
		ClearReservation(ResSeat);
		return false;
	}

	//	좌석·손패·역할·생사를 통째로 새 PlayerState 로 옮긴다.
	OldPS->CopyProperties(NewPS);
	NewPS->SetDisconnected(false);

	UE_LOG(LogBGNet, Log, TEXT("[Seat] 좌석 %d 복귀 — %s (손패 %d 장)"),
		NewPS->GetSeatIndex(), *NewPS->GetPlayerName(), NewPS->GetHandCount());

	//	옛 상태는 여기서 끝난다 — 남겨 두면 같은 좌석이 둘이 된다.
	OldPS->Destroy();

	//	예약은 아직 지우지 않는다. 두고 간 폰을 다시 물리는 단계(RepossessAbandonedPawn)가 남았다.
	for (FBGSeatReservation& R : Reservations)
	{
		if (R.Seat == ResSeat)
		{
			R.State = NewPS;
			break;
		}
	}

	return true;
}

bool UBGSeatComponent::RepossessAbandonedPawn(APlayerController* Rejoining)
{
	ABGPlayerState* PS = Rejoining ? Rejoining->GetPlayerState<ABGPlayerState>() : nullptr;
	if (!PS)
	{
		return false;
	}

	const int32 Seat = PS->GetSeatIndex();
	FBGSeatReservation* Res = Reservations.FindByPredicate(
		[Seat](const FBGSeatReservation& R) { return R.Seat == Seat; });

	//	예약이 없다 = 처음 들어온 사람이다. 정상 경로이므로 조용히 넘어간다.
	if (!Res)
	{
		return false;
	}

	APawn* Abandoned = Res->Pawn.Get();

	//	예약은 여기서 끝난다 — 폰을 물리든 못 물리든 이 사람은 돌아왔다.
	Reservations.RemoveAll([Seat](const FBGSeatReservation& R) { return R.Seat == Seat; });
	PS->ClearAbandonedPawn();

	//	폰이 이미 사라졌다면 새로 스폰한다(역할·체력은 스냅샷에서 되돌린다).
	if (!IsValid(Abandoned))
	{
		return false;
	}

	if (Abandoned->GetController() != nullptr)
	{
		//	누가 물고 있는 폰은 건드리지 않는다. 새 폰이 스폰되겠지만 겹치지는 않는다.
		UE_LOG(LogBGNet, Warning, TEXT("[Seat] 좌석 %d 두고 간 폰에 이미 컨트롤러가 있다 — 재빙의 포기"), Seat);
		return false;
	}

	Rejoining->Possess(Abandoned);

	//	폰을 그대로 물었으니 체력·장비 GE 가 이미 옳다. 스냅샷을 덮어씌우면 이중 적용된다.
	PS->DiscardSavedVitals();

	UE_LOG(LogBGNet, Log, TEXT("[Seat] 좌석 %d 두고 간 폰 재빙의 — %s"),
		Seat, *GetNameSafe(Abandoned));

	//	자리를 비운 사이 원이 좁아졌을 수 있다 — 좌석 위치를 다시 맞춘다.
	SnapPawnToSeat(PS);
	return true;
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

	// 번호를 전부 바꾼 뒤에 옮긴다. 한 명씩 "번호 바꾸고 바로 옮기기" 를 하면 중간 상태에서
	// 같은 번호를 둘이 갖게 되고, 자리 각도는 그 목록으로 계산되어 둘이 한 자리에 겹친다.
	for (int32 i = 0; i < Take; ++i)
	{
		States[i]->SetSeatIndex(Seats[i]);

		//	자리 위치는 캠프파이어가 정한다 — PS_Seat* 유무는 여기서 볼 일이 아니다.
		UE_LOG(LogBGNet, Log, TEXT("[Seat] 랜덤 배정 — %s → 좌석 %d"),
			*States[i]->GetPlayerName(), Seats[i]);
	}

	// 두 번 돌린다. 첫 순회에서는 아직 안 옮긴 폰이 목적지에 서 있을 수 있어 TeleportTo 가
	// 옆으로 밀어낸다. 그때는 전원이 제 자리 근처에 와 있으므로 두 번째에 정확히 앉는다.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 i = 0; i < Take; ++i)
		{
			SnapPawnToSeat(States[i]);
		}
	}
}

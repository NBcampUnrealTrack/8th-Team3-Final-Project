#include "Core/BGCampfire.h"

#include "Components/StaticMeshComponent.h"
#include "Core/BGPlayerState.h"
#include "Debug/BGCardLog.h"
#include "EngineUtils.h"            // TActorIterator
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetMathLibrary.h"   // FindLookAtRotation

ABGCampfire::ABGCampfire()
{
	//	위치 기준 액터다. 배치는 인원이 바뀔 때만 일어나므로 매 프레임 돌 이유가 없다.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	FireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FireMesh"));
	FireMesh->SetupAttachment(Root);
	//	플레이어가 불에 끼어 밀려나지 않도록. 판의 중심은 통과 가능해야 한다.
	FireMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

ABGCampfire* ABGCampfire::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	ABGCampfire* Found = nullptr;
	for (TActorIterator<ABGCampfire> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (!Found)
		{
			Found = *It;
			continue;
		}

		//	둘 이상이면 어느 쪽 주위에 앉는지가 배치 순서에 좌우된다 — 반드시 알려야 한다.
		UE_LOG(LogBGCard, Warning,
			TEXT("[Campfire] 레벨에 캠프파이어가 둘 이상입니다(%s, %s ...). 첫 번째를 사용합니다."),
			*Found->GetName(), *It->GetName());
		break;
	}

	return Found;
}

FTransform ABGCampfire::GetSeatTransform(int32 Ordinal, int32 SeatCount) const
{
	if (SeatCount <= 0)
	{
		return GetActorTransform();
	}

	//	인원수로 각도를 나눈다 — 4명이면 90도, 7명이면 약 51.43도.
	const float StepDeg  = GetAngleStepDeg(SeatCount);
	const float Direction = bClockwise ? -1.f : 1.f;
	const float AngleDeg  = FirstSeatAngleDeg + Direction * StepDeg * Ordinal;
	const float AngleRad  = FMath::DegreesToRadians(AngleDeg);

	const FVector Center = GetActorLocation();
	const FVector Offset(
		FMath::Cos(AngleRad) * SeatRadius,
		FMath::Sin(AngleRad) * SeatRadius,
		SeatHeightOffset);

	const FVector SeatLocation = Center + Offset;

	//	캠프파이어를 바라보게 한다. 요만 쓰는 이유: 높이차가 있으면 피치가 생겨 캐릭터가 기운다.
	FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(SeatLocation, Center);
	LookAt.Pitch = 0.f;
	LookAt.Roll  = 0.f;

	return FTransform(LookAt, SeatLocation, FVector::OneVector);
}

void ABGCampfire::CollectOccupiedSeats(TArray<int32>& OutSeats) const
{
	OutSeats.Reset();

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}

	for (APlayerState* Base : GS->PlayerArray)
	{
		const ABGPlayerState* PS = Cast<ABGPlayerState>(Base);
		if (PS && PS->GetSeatIndex() != INDEX_NONE)
		{
			OutSeats.Add(PS->GetSeatIndex());
		}
	}

	//	번호순으로 정렬해야 좌석 번호가 커질수록 원을 한 방향으로 도는 배치가 된다.
	//	PlayerArray 순서는 접속 순이라 그대로 쓰면 자리가 뒤죽박죽 섞인다.
	OutSeats.Sort();
}

int32 ABGCampfire::GetSeatedPlayerCount() const
{
	TArray<int32> Seats;
	CollectOccupiedSeats(Seats);
	return Seats.Num();
}

bool ABGCampfire::GetTransformForSeat(int32 SeatIndex, FTransform& OutTransform) const
{
	TArray<int32> Seats;
	CollectOccupiedSeats(Seats);

	//	좌석 "번호" 가 아니라 정렬된 목록에서의 "순번" 으로 각도를 매긴다.
	//	로비의 듬성듬성한 번호(0,2,4)도 균등하게 벌어진다.
	const int32 Ordinal = Seats.IndexOfByKey(SeatIndex);
	if (Ordinal == INDEX_NONE)
	{
		return false;
	}

	OutTransform = GetSeatTransform(Ordinal, Seats.Num());
	return true;
}

void ABGCampfire::ArrangePlayers()
{
	if (!HasAuthority())
	{
		//	폰 트랜스폼은 서버가 옮기고 복제된다. 클라가 옮기면 다음 복제에 되돌아간다.
		return;
	}

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}

	TArray<int32> Seats;
	CollectOccupiedSeats(Seats);

	const int32 SeatCount = Seats.Num();
	if (SeatCount == 0)
	{
		return;
	}

	int32 Placed = 0;

	for (APlayerState* Base : GS->PlayerArray)
	{
		ABGPlayerState* PS = Cast<ABGPlayerState>(Base);
		if (!PS || PS->GetSeatIndex() == INDEX_NONE)
		{
			continue;
		}

		APawn* Pawn = PS->GetPawn();
		if (!Pawn)
		{
			//	폰이 아직 안 왔다. 다음 호출에서 잡히므로 경고까지 할 일은 아니다.
			continue;
		}

		const int32 Ordinal = Seats.IndexOfByKey(PS->GetSeatIndex());
		if (Ordinal == INDEX_NONE)
		{
			continue;
		}

		const FTransform SeatTransform = GetSeatTransform(Ordinal, SeatCount);
		const FVector    Loc = SeatTransform.GetLocation();
		const FRotator   Rot = SeatTransform.GetRotation().Rotator();

		//	TeleportTo 는 겹침을 밀어낸다 — SetActorLocation 은 자리에 끼일 수 있다.
		//	(UBGSeatComponent::SnapPawnToSeat 과 같은 이유로 같은 함수를 쓴다)
		Pawn->TeleportTo(Loc, Rot);

		//	컨트롤 회전까지 맞춰야 시선이 캠프파이어를 향한다.
		if (AController* C = Pawn->GetController())
		{
			C->SetControlRotation(Rot);
		}

		++Placed;
	}

	UE_LOG(LogBGCard, Log,
		TEXT("[Campfire] %d명 배치 — 반지름 %.0f, 간격 %.1f도"),
		Placed, SeatRadius, GetAngleStepDeg(SeatCount));
}

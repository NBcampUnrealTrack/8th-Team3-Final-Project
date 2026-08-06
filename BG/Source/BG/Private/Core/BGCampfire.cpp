#include "Core/BGCampfire.h"

#include "Components/StaticMeshComponent.h"
#include "Core/BGCharacter.h"       // SetBodyFacing — 몸 회전은 캐릭터가 처리한다
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

FTransform ABGCampfire::GetSeatTransform(int32 Slot, int32 SlotCount) const
{
	if (SlotCount <= 0)
	{
		return GetActorTransform();
	}

	//	자리 수는 정원(7)으로 고정이다 — 레벨의 의자가 그 간격으로 놓여 있다.
	const float StepDeg  = GetAngleStepDeg(SlotCount);
	const float Direction = bClockwise ? -1.f : 1.f;
	const float AngleDeg  = FirstSeatAngleDeg + Direction * StepDeg * Slot;
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

	//	좌석 "번호" 가 아니라 정렬된 목록에서의 "순번" 으로 각도를 매긴다 — 4인이면 90도로
	//	고르게 벌어진다. 로비의 7자리 고정 배치는 레벨의 PS_Seat* 가 맡는다.
	const int32 Ordinal = Seats.IndexOfByKey(SeatIndex);
	if (Ordinal == INDEX_NONE)
	{
		return false;
	}

	OutTransform = GetSeatTransform(Ordinal, Seats.Num());
	return true;
}

bool ABGCampfire::PlacePawn(APawn* Pawn, const FTransform& SeatTransform) const
{
	if (!Pawn)
	{
		return false;
	}

	FVector        Loc = SeatTransform.GetLocation();
	const FRotator Rot = SeatTransform.GetRotation().Rotator();

	//	좌석 트랜스폼은 "바닥의 한 점" 이다. 캡슐 중심을 반높이만큼 올리지 않으면
	//	캡슐 아래 절반이 지면에 파묻힌 상태가 되고, FindTeleportSpot 이 빠져나올 자리를
	//	찾지 못해 TeleportTo 가 false 를 돌려준다(액터는 움직이지 않는다).
	//	폰마다 캡슐 크기가 다를 수 있으므로 상수를 쓰지 않고 폰에게 물어본다.
	Loc.Z += Pawn->GetSimpleCollisionHalfHeight();

	//	TeleportTo 는 겹침을 밀어낸다 — SetActorLocation 은 자리에 끼일 수 있다.
	if (!Pawn->TeleportTo(Loc, Rot))
	{
		//	반환값을 버리면 "배치했다" 는 로그만 남고 캐릭터는 제자리에 있는 상태가 된다.
		//	실제로 그 상태로 한참 헤맸다 — 실패는 반드시 눈에 보여야 한다.
		UE_LOG(LogBGCard, Warning,
			TEXT("[Campfire] %s 를 (%.0f, %.0f, %.0f) 로 옮기지 못했습니다 — 그 자리가 막혀 있습니다. "
			     "캠프파이어 높이나 SeatRadius 를 확인하세요(지형 안/절벽 밖일 수 있습니다)."),
			*Pawn->GetName(), Loc.X, Loc.Y, Loc.Z);
		return false;
	}

	//	── 몸을 캠프파이어 쪽으로 돌린다 ──
	//	TeleportTo 도 회전을 넣지만 그것만으로는 부족하다: 캐릭터의 액터 회전은
	//	기본 설정에서 매 프레임 컨트롤 회전으로 덮이고, 소유 클라에는 복제되지도 않는다.
	//	ABGCharacter 가 그 두 가지를 함께 처리한다.
	if (ABGCharacter* Character = Cast<ABGCharacter>(Pawn))
	{
		Character->SetBodyFacing(Rot);
	}
	else
	{
		//	ABGCharacter 가 아닌 폰(테스트용 기본 폰 등). 액터 회전만 맞춰 둔다.
		Pawn->SetActorRotation(Rot);
	}

	//	시선 시작 방향도 캠프파이어로 맞춘다. 몸 회전과 달리 이건 카메라 쪽 값이다.
	if (AController* C = Pawn->GetController())
	{
		C->SetControlRotation(Rot);
	}

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

		FTransform SeatTransform;
		if (!GetTransformForSeat(PS->GetSeatIndex(), SeatTransform))
		{
			continue;
		}

		if (PlacePawn(Pawn, SeatTransform))
		{
			++Placed;
		}
	}

	//	성공한 수와 좌석 수를 함께 찍는다. 둘이 다르면 폰이 없거나 자리가 막힌 것이고,
	//	성공 수만 찍으면 "배치했다는데 안 움직인다" 를 구분할 수 없다.
	UE_LOG(LogBGCard, Log,
		TEXT("[Campfire] %d/%d명 배치 — 반지름 %.0f, 간격 %.1f도"),
		Placed, SeatCount, SeatRadius, GetAngleStepDeg(SeatCount));
}

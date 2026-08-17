// TRLevelSceneManager.cpp

#include "Environment/TRLevelSceneManager.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TRGameState.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Environment/TRLevelSceneChunk.h"
#include "Environment/TRLevelSceneFloor.h"
#include "Environment/TRLevelSceneRailSegment.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

//생성자
ATRLevelSceneManager::ATRLevelSceneManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	ChunkClass = ATRLevelSceneChunk::StaticClass();
	RailSegmentClass = ATRLevelSceneRailSegment::StaticClass();
	FloorClass = ATRLevelSceneFloor::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FloorMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	FloorMesh = FloorMeshFinder.Object;
}

//게임 시작시 환경 메시 청크와 레일, 지면 생성/설정.
void ATRLevelSceneManager::BeginPlay()
{
	Super::BeginPlay();
	ValidateGenerationSettings();
	CreateInitialChunks();
	CreateInitialRailSegments();
	CreateFloor();
}

//세팅 검증용 코드
void ATRLevelSceneManager::ValidateGenerationSettings() const
{
	//처음 뒤에 배치할 청크 액터 갯수가 전체를 넘어서는지
	const int32 SafeChunkCount = FMath::Max(1, ChunkCount);
	if (InitialBehindChunkCount >= SafeChunkCount)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: InitialBehindChunkCount(%d) must be less than ChunkCount(%d). Runtime value will be clamped to %d."),
			*GetName(), InitialBehindChunkCount, ChunkCount, SafeChunkCount - 1);
	}

	//처음 뒤에 배치할 레일 액터 갯수가 전체를 넘어서는지
	const int32 SafeRailSegmentCount = FMath::Max(1, RailSegmentCount);
	if (InitialBehindRailSegmentCount >= SafeRailSegmentCount)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: InitialBehindRailSegmentCount(%d) must be less than RailSegmentCount(%d). Runtime value will be clamped to %d."),
			*GetName(), InitialBehindRailSegmentCount, RailSegmentCount, SafeRailSegmentCount - 1);
	}

	//레일 생성 조건 맞는지 확인
	const bool bExcludeRailArea = bGenerateRail && RailSegmentClass && RailMesh;
	if (!bExcludeRailArea)
	{
		return;
	}

	//LateralRange스폰 범위가 레일 스폰 금지 구역에 완전 가려지는지 확인
	const float HalfWidth = FMath::Max(1.f, ChunkSize.Y) * 0.5f;
	const float RailClearance = FMath::Max(0.f, RailPlacementClearanceHalfWidth);
	const FVector2D RailLateralExclusionRange(RailOffset.Y - RailClearance, RailOffset.Y + RailClearance);

	for (int32 DefinitionIndex = 0; DefinitionIndex < MeshDefinitions.Num(); ++DefinitionIndex)
	{
		const FTRLevelSceneMeshDefinition& Definition = MeshDefinitions[DefinitionIndex];
		if (Definition.PlacementMode != ETRLevelScenePlacementMode::Random)
		{
			continue;
		}

		const float MinY = FMath::Clamp(
			FMath::Min(Definition.LateralRange.X, Definition.LateralRange.Y), -HalfWidth, HalfWidth);
		const float MaxY = FMath::Clamp(
			FMath::Max(Definition.LateralRange.X, Definition.LateralRange.Y), MinY, HalfWidth);
		const float ExclusionMin = FMath::Min(RailLateralExclusionRange.X, RailLateralExclusionRange.Y);
		const float ExclusionMax = FMath::Max(RailLateralExclusionRange.X, RailLateralExclusionRange.Y);

		bool bHasAllowedPlacement = true;
		if (FMath::IsNearlyEqual(MinY, MaxY))
		{
			bHasAllowedPlacement = MinY < ExclusionMin || MinY > ExclusionMax;
		}
		else
		{
			const float OverlapMin = FMath::Max(MinY, ExclusionMin);
			const float OverlapMax = FMath::Min(MaxY, ExclusionMax);
			if (OverlapMin < OverlapMax)
			{
				const float TotalAllowedWidth = FMath::Max(0.f, OverlapMin - MinY)
					+ FMath::Max(0.f, MaxY - OverlapMax);
				bHasAllowedPlacement = TotalAllowedWidth > KINDA_SMALL_NUMBER;
			}
		}

		if (!bHasAllowedPlacement)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("%s: MeshDefinitions[%d] cannot spawn. Effective lateral range [%.2f, %.2f] is fully covered by rail exclusion [%.2f, %.2f]."),
				*GetName(), DefinitionIndex, MinY, MaxY, ExclusionMin, ExclusionMax);
		}
	}
}

//게임 종료시 관리 대상 액터 전체 삭제
void ATRLevelSceneManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ATRLevelSceneChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			Chunk->Destroy();
		}
	}
	ActiveChunks.Reset();
	for (ATRLevelSceneRailSegment* RailSegment : ActiveRailSegments)
	{
		if (IsValid(RailSegment))
		{
			RailSegment->Destroy();
		}
	}
	ActiveRailSegments.Reset();

	if (IsValid(ActiveFloor))
	{
		ActiveFloor->Destroy();
	}
	ActiveFloor = nullptr;

	Super::EndPlay(EndPlayReason);
}

//틱마다 청크/레일/지면 업데이트 및 필요시 재활용
void ATRLevelSceneManager::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFloorVelocity();

	if (!IsSceneMovementAllowed() || VisualSpeed <= 0.f)
	{
		return;
	}

	MoveChunks(DeltaSeconds);
	MoveRailSegments(DeltaSeconds);
	RecyclePassedChunks();
	RecyclePassedRailSegments();
}

//배경을 흘려도 되는 상태인지 판정
bool ATRLevelSceneManager::IsSceneMovementAllowed() const
{
	if (!bAutoMove)
	{
		return false;
	}

	if (!bMoveOnlyAfterMatchStart)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const ATRGameState* State = World ? World->GetGameState<ATRGameState>() : nullptr;

	return State != nullptr && State->IsMatchStarted();
}

//이동속도 설정
void ATRLevelSceneManager::SetVisualSpeed(const float NewVisualSpeed)
{
	VisualSpeed = FMath::Max(0.f, NewVisualSpeed);
	UpdateFloorVelocity(true);
}

//레일 조각 액터 생성
void ATRLevelSceneManager::CreateInitialRailSegments()
{
	if (!bGenerateRail || !GetWorld() || !RailSegmentClass || !RailMesh)
	{
		return;
	}

	const int32 SafeSegmentCount = FMath::Max(1, RailSegmentCount);
	const int32 SafeBehindCount = FMath::Clamp(InitialBehindRailSegmentCount, 0, SafeSegmentCount - 1);
	const float SafeSegmentLength = FMath::Max(1.f, RailSegmentLength);
	const FVector TravelDirection = GetActorForwardVector();
	const FVector WorldRailOffset = GetActorTransform().TransformVectorNoScale(RailOffset);

	ActiveRailSegments.Reserve(SafeSegmentCount);
	for (int32 SegmentIndex = 0; SegmentIndex < SafeSegmentCount; ++SegmentIndex)
	{
		const float TravelOffset = static_cast<float>(SegmentIndex - SafeBehindCount) * SafeSegmentLength;
		const FTransform SpawnTransform(GetActorRotation(),
			GetActorLocation() + WorldRailOffset + TravelDirection * TravelOffset);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATRLevelSceneRailSegment* RailSegment = GetWorld()->SpawnActor<ATRLevelSceneRailSegment>(
			RailSegmentClass, SpawnTransform, SpawnParameters);
		if (RailSegment)
		{
			RailSegment->InitializeRail(RailMesh, RailEndCullDistance, bRailCastShadow);
			ActiveRailSegments.Add(RailSegment);
		}
	}
}

//지면 액터 생성
void ATRLevelSceneManager::CreateFloor()
{
	if (!bGenerateFloor || !GetWorld() || !FloorClass)
	{
		return;
	}

	//설정되어 있으면 이미 존재하는 액터로 설정
	UStaticMeshComponent* ExistingFloorMeshComponent = nullptr;
	if (bUseExistingLevelFloor && IsValid(ExistingFloorActor))
	{
		ExistingFloorMeshComponent = ExistingFloorActor->FindComponentByClass<UStaticMeshComponent>();
	}

	if (!ExistingFloorMeshComponent && !FloorMesh)
	{
		return;
	}

	const FTransform SpawnTransform = ExistingFloorMeshComponent
		? FTransform::Identity
		: FloorRelativeTransform * GetActorTransform();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveFloor = GetWorld()->SpawnActor<ATRLevelSceneFloor>(FloorClass, SpawnTransform, SpawnParameters);
	if (ActiveFloor)
	{
		if (ExistingFloorMeshComponent)
		{
			ActiveFloor->InitializeExistingFloor(ExistingFloorMeshComponent);
			UE_LOG(LogTemp, Display, TEXT("Floor material target: existing level actor %s"),
				*ExistingFloorActor->GetName());
		}
		else
		{
			ActiveFloor->InitializeFloor(FloorMesh, FloorMaterial);
			UE_LOG(LogTemp, Display, TEXT("Floor material target: generated floor plane"));
		}
		UpdateFloorVelocity(true);
	}
}

//기본 시드값 설정 및 필요시 청크 초기화
void ATRLevelSceneManager::SetRouteSeed(const int32 NewRouteSeed, const bool bRegenerate)
{
	RouteSeed = NewRouteSeed;
	if (bRegenerate)
	{
		RegenerateAllChunks();
	}
}

//레일 조각 액터 속도만큼 이동
void ATRLevelSceneManager::MoveRailSegments(const float DeltaSeconds)
{
	const FVector MovementDelta = -GetActorForwardVector() * VisualSpeed * DeltaSeconds;
	for (ATRLevelSceneRailSegment* RailSegment : ActiveRailSegments)
	{
		if (IsValid(RailSegment))
		{
			RailSegment->AddActorWorldOffset(MovementDelta, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

//활성화된 청크들의 ISM 인스턴스 리셋, 청크 위치는 변경하지 않음.
void ATRLevelSceneManager::RegenerateAllChunks()
{
	for (ATRLevelSceneChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			Chunk->GenerateChunk(MakeChunkSeed(Chunk->GetSegmentIndex()), Chunk->GetSegmentIndex());
		}
	}
}

//지나간 레일 액터 확인
void ATRLevelSceneManager::RecyclePassedRailSegments()
{
	const float SafeSegmentLength = FMath::Max(1.f, RailSegmentLength);
	const float RecycleThreshold = -FMath::Max(0.f, RailRecycleBehindDistance) - SafeSegmentLength * 0.5f;
	int32 RecycledSegmentCount = 0;

	while (RecycledSegmentCount < ActiveRailSegments.Num())
	{
		ATRLevelSceneRailSegment* PassedSegment = nullptr;
		for (ATRLevelSceneRailSegment* RailSegment : ActiveRailSegments)
		{
			if (IsValid(RailSegment) && GetRailTravelDistance(RailSegment) < RecycleThreshold)
			{
				PassedSegment = RailSegment;
				break;
			}
		}

		if (!PassedSegment)
		{
			break;
		}

		RecycleRailSegment(PassedSegment);
		++RecycledSegmentCount;
	}
}

//청크 초기 세팅
void ATRLevelSceneManager::CreateInitialChunks()
{
	if (!GetWorld() || !ChunkClass)
	{
		return;
	}

	const int32 SafeChunkCount = FMath::Max(1, ChunkCount);
	const int32 SafeBehindCount = FMath::Clamp(InitialBehindChunkCount, 0, SafeChunkCount - 1);
	const float ChunkLength = FMath::Max(1.f, ChunkSize.X);
	const FVector TravelDirection = GetActorForwardVector();
	const bool bExcludeRailArea = bGenerateRail && RailSegmentClass && RailMesh;
	const FVector2D RailLateralExclusionRange(
		RailOffset.Y - FMath::Max(0.f, RailPlacementClearanceHalfWidth),
		RailOffset.Y + FMath::Max(0.f, RailPlacementClearanceHalfWidth));

	ActiveChunks.Reserve(SafeChunkCount);
	for (int32 ChunkIndex = 0; ChunkIndex < SafeChunkCount; ++ChunkIndex)
	{
		const float TravelOffset = static_cast<float>(ChunkIndex - SafeBehindCount) * ChunkLength;
		const FTransform SpawnTransform(GetActorRotation(), GetActorLocation() + TravelDirection * TravelOffset);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATRLevelSceneChunk* Chunk = GetWorld()->SpawnActor<ATRLevelSceneChunk>(ChunkClass, SpawnTransform, SpawnParameters);
		if (!Chunk)
		{
			continue;
		}

		const int32 SegmentIndex = ChunkIndex;
		Chunk->InitializeChunk(MeshDefinitions, ChunkSize, bExcludeRailArea,
			RailLateralExclusionRange);
		Chunk->GenerateChunk(MakeChunkSeed(SegmentIndex), SegmentIndex);
		ActiveChunks.Add(Chunk);
	}

	NextSegmentIndex = SafeChunkCount;
}

//레일 액터 재활용하여 앞쪽에 다시 배치
void ATRLevelSceneManager::RecycleRailSegment(ATRLevelSceneRailSegment* RailSegment)
{
	if (!IsValid(RailSegment))
	{
		return;
	}

	const float NewTravelDistance = GetFrontmostRailTravelDistance(RailSegment) + FMath::Max(1.f, RailSegmentLength);
	const FVector WorldRailOffset = GetActorTransform().TransformVectorNoScale(RailOffset);
	const FVector NewLocation = GetActorLocation() + WorldRailOffset + GetActorForwardVector() * NewTravelDistance;
	RailSegment->SetActorLocationAndRotation(NewLocation, GetActorRotation(), false, nullptr,
		ETeleportType::TeleportPhysics);
}

//지면 머티리얼 속도 파라미터 업데이트
void ATRLevelSceneManager::UpdateFloorVelocity(const bool bForceUpdate)
{
	if (!IsValid(ActiveFloor))
	{
		return;
	}

	const FVector2D SafeDirection = FloorMovementDirection.GetSafeNormal();
	const float EffectiveVisualSpeed = IsSceneMovementAllowed() ? FMath::Max(0.f, VisualSpeed) : 0.f;
	if (!bForceUpdate
		&& FMath::IsNearlyEqual(LastAppliedFloorVisualSpeed, EffectiveVisualSpeed)
		&& LastAppliedFloorMovementDirection.Equals(SafeDirection))
	{
		return;
	}

	const FVector2D MaterialVelocity = SafeDirection * EffectiveVisualSpeed;
	ActiveFloor->SetMovementVelocity(MaterialVelocity.X, MaterialVelocity.Y);
	LastAppliedFloorVisualSpeed = EffectiveVisualSpeed;
	LastAppliedFloorMovementDirection = SafeDirection;
}

//청크 속도만큼 이동
void ATRLevelSceneManager::MoveChunks(const float DeltaSeconds)
{
	const FVector MovementDelta = -GetActorForwardVector() * VisualSpeed * DeltaSeconds;
	for (ATRLevelSceneChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			Chunk->AddActorWorldOffset(MovementDelta, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

//매니저 액터 기준 레일 액터의 위치, 앞인지 뒤인지 계산
float ATRLevelSceneManager::GetRailTravelDistance(const ATRLevelSceneRailSegment* RailSegment) const
{
	if (!IsValid(RailSegment))
	{
		return -TNumericLimits<float>::Max();
	}

	const FVector WorldRailOffset = GetActorTransform().TransformVectorNoScale(RailOffset);
	return FVector::DotProduct(RailSegment->GetActorLocation() - GetActorLocation() - WorldRailOffset, GetActorForwardVector());
}

//지나간 청크 액터 확인
void ATRLevelSceneManager::RecyclePassedChunks()
{
	const float RecycleThreshold = -FMath::Max(0.f, RecycleBehindDistance) - FMath::Max(1.f, ChunkSize.X) * 0.5f;
	int32 RecycledChunkCount = 0;

	while (RecycledChunkCount < ActiveChunks.Num())
	{
		ATRLevelSceneChunk* PassedChunk = nullptr;
		for (ATRLevelSceneChunk* Chunk : ActiveChunks)
		{
			if (IsValid(Chunk) && GetTravelDistance(Chunk) < RecycleThreshold)
			{
				PassedChunk = Chunk;
				break;
			}
		}

		if (!PassedChunk)
		{
			break;
		}

		RecycleChunk(PassedChunk);
		++RecycledChunkCount;
	}
}

//가장 앞에 있는 레일 액터까지의 거리 확인
float ATRLevelSceneManager::GetFrontmostRailTravelDistance(const ATRLevelSceneRailSegment* IgnoredRailSegment) const
{
	float FrontmostDistance = 0.f;
	bool bFoundSegment = false;
	for (const ATRLevelSceneRailSegment* RailSegment : ActiveRailSegments)
	{
		if (!IsValid(RailSegment) || RailSegment == IgnoredRailSegment)
		{
			continue;
		}

		FrontmostDistance = bFoundSegment
			? FMath::Max(FrontmostDistance, GetRailTravelDistance(RailSegment))
			: GetRailTravelDistance(RailSegment);
		bFoundSegment = true;
	}

	return bFoundSegment ? FrontmostDistance : 0.f;
}

//청크 액터 재활용해 앞에 다시 배치
void ATRLevelSceneManager::RecycleChunk(ATRLevelSceneChunk* Chunk)
{
	if (!IsValid(Chunk))
	{
		return;
	}

	const float ChunkLength = FMath::Max(1.f, ChunkSize.X);
	const float NewTravelDistance = GetFrontmostTravelDistance(Chunk) + ChunkLength;
	const FVector NewLocation = GetActorLocation() + GetActorForwardVector() * NewTravelDistance;
	Chunk->SetActorLocationAndRotation(NewLocation, GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);

	const int32 SegmentIndex = NextSegmentIndex++;
	Chunk->GenerateChunk(MakeChunkSeed(SegmentIndex), SegmentIndex);
}

//매니저 위치 기준 청크의 상대 위치, 앞인지 뒤인지 확인
float ATRLevelSceneManager::GetTravelDistance(const ATRLevelSceneChunk* Chunk) const
{
	if (!IsValid(Chunk))
	{
		return -TNumericLimits<float>::Max();
	}

	return FVector::DotProduct(Chunk->GetActorLocation() - GetActorLocation(), GetActorForwardVector());
}

//가장 앞에 있는 청크까지의 거리 확인
float ATRLevelSceneManager::GetFrontmostTravelDistance(const ATRLevelSceneChunk* IgnoredChunk) const
{
	float FrontmostDistance = 0.f;
	bool bFoundChunk = false;
	for (const ATRLevelSceneChunk* Chunk : ActiveChunks)
	{
		if (!IsValid(Chunk) || Chunk == IgnoredChunk)
		{
			continue;
		}

		FrontmostDistance = bFoundChunk
			? FMath::Max(FrontmostDistance, GetTravelDistance(Chunk))
			: GetTravelDistance(Chunk);
		bFoundChunk = true;
	}

	return bFoundChunk ? FrontmostDistance : 0.f;
}

//기본 시드값 기반하여 청크 액터마다 가지는 개별 시드값 생성
int32 ATRLevelSceneManager::MakeChunkSeed(const int32 SegmentIndex) const
{
	return static_cast<int32>(HashCombine(GetTypeHash(RouteSeed), GetTypeHash(SegmentIndex)));
}

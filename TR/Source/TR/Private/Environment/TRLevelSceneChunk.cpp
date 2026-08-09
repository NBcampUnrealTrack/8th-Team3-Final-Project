// TRLevelSceneChunk.cpp

#include "Environment/TRLevelSceneChunk.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"

//생성자 
ATRLevelSceneChunk::ATRLevelSceneChunk()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

//청크 정보 초기설정 함수
void ATRLevelSceneChunk::InitializeChunk(const TArray<FTRLevelSceneMeshDefinition>& InMeshDefinitions, const FVector2D& InChunkSize, const bool bInExcludeRailAreaFromRandomPlacement, const FVector2D& InRailLateralExclusionRange)
{
	MeshDefinitions = InMeshDefinitions;
	ChunkSize.X = FMath::Max(1.f, InChunkSize.X);
	ChunkSize.Y = FMath::Max(1.f, InChunkSize.Y);
	bExcludeRailAreaFromRandomPlacement = bInExcludeRailAreaFromRandomPlacement;
	RailLateralExclusionRange = InRailLateralExclusionRange;
	RebuildInstanceComponents();
}

// 청크 정보 리셋 함수, 시드에 기반해 업데이트
void ATRLevelSceneChunk::GenerateChunk(const int32 InChunkSeed, const int32 InSegmentIndex)
{
	SegmentIndex = InSegmentIndex;
	ClearInstances();

	for (int32 DefinitionIndex = 0; DefinitionIndex < MeshDefinitions.Num(); ++DefinitionIndex)
	{
		const FTRLevelSceneMeshDefinition& Definition = MeshDefinitions[DefinitionIndex];
		if (!Definition.Mesh || !SceneInstanceComponents.IsValidIndex(DefinitionIndex))
		{
			continue;
		}

		const int32 MinCount = FMath::Max(0, Definition.MinInstanceCount);
		const int32 MaxCount = FMath::Max(MinCount, Definition.MaxInstanceCount);
		const int32 DefinitionSeed = static_cast<int32>(HashCombine(GetTypeHash(InChunkSeed), GetTypeHash(DefinitionIndex)));
		FRandomStream CountStream(DefinitionSeed);
		const int32 InstanceCount = CountStream.RandRange(MinCount, MaxCount);

		UInstancedStaticMeshComponent* InstanceComponent = SceneInstanceComponents[DefinitionIndex];
		InstanceComponent->PreAllocateInstancesMemory(InstanceCount);

		for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
		{
			FTransform InstanceTransform;
			if (CreateInstanceTransform(Definition, DefinitionIndex, InstanceIndex, InstanceCount, InChunkSeed,
				InstanceTransform))
			{
				InstanceComponent->AddInstance(InstanceTransform);
			}
		}
	}
}

// ISM 초기화 함수
void ATRLevelSceneChunk::RebuildInstanceComponents()
{
	for (UInstancedStaticMeshComponent* InstanceComponent : SceneInstanceComponents)
	{
		if (IsValid(InstanceComponent))
		{
			InstanceComponent->DestroyComponent();
		}
	}
	SceneInstanceComponents.Reset();
	SceneInstanceComponents.Reserve(MeshDefinitions.Num());

	for (int32 DefinitionIndex = 0; DefinitionIndex < MeshDefinitions.Num(); ++DefinitionIndex)
	{
		const FTRLevelSceneMeshDefinition& Definition = MeshDefinitions[DefinitionIndex];
		const FName ComponentName = MakeUniqueObjectName(
			this, UInstancedStaticMeshComponent::StaticClass(),
			*FString::Printf(TEXT("SceneISM_%d"), DefinitionIndex));

		UInstancedStaticMeshComponent* InstanceComponent = NewObject<UInstancedStaticMeshComponent>(this, ComponentName);
		InstanceComponent->SetupAttachment(SceneRoot);
		InstanceComponent->SetMobility(EComponentMobility::Movable);
		InstanceComponent->SetStaticMesh(Definition.Mesh);
		InstanceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InstanceComponent->SetGenerateOverlapEvents(false);
		InstanceComponent->SetCanEverAffectNavigation(false);
		InstanceComponent->SetCastShadow(Definition.bCastShadow);
		InstanceComponent->SetCullDistances(Definition.StartCullDistance, Definition.EndCullDistance);
		AddInstanceComponent(InstanceComponent);
		InstanceComponent->RegisterComponent();
		SceneInstanceComponents.Add(InstanceComponent);
	}
}

// ISM 클리어 함수
void ATRLevelSceneChunk::ClearInstances()
{
	for (UInstancedStaticMeshComponent* InstanceComponent : SceneInstanceComponents)
	{
		if (IsValid(InstanceComponent))
		{
			InstanceComponent->ClearInstances();
		}
	}
}

//ISM에 추가할 인스턴스 시드에 기반해 트랜스폼값 생성
bool ATRLevelSceneChunk::CreateInstanceTransform(const FTRLevelSceneMeshDefinition& Definition, const int32 DefinitionIndex, const int32 InstanceIndex, const int32 InstanceCount, const int32 ChunkSeed, FTransform& OutTransform) const
{
	const int32 DefinitionSeed = static_cast<int32>(HashCombine(GetTypeHash(ChunkSeed), GetTypeHash(DefinitionIndex)));
	const int32 ObjectSeed = static_cast<int32>(HashCombine(GetTypeHash(DefinitionSeed), GetTypeHash(InstanceIndex)));
	FRandomStream Stream(ObjectSeed);

	const float HalfLength = ChunkSize.X * 0.5f;
	float LocalX = Stream.FRandRange(-HalfLength, HalfLength);
	if (Definition.PlacementMode == ETRLevelScenePlacementMode::UniformAlongLength && InstanceCount > 0)
	{
		const float Alpha = (static_cast<float>(InstanceIndex) + 0.5f) / static_cast<float>(InstanceCount);
		LocalX = FMath::Lerp(-HalfLength, HalfLength, Alpha)
			+ Stream.FRandRange(-Definition.LongitudinalJitter, Definition.LongitudinalJitter);
	}
	LocalX = FMath::Clamp(LocalX, -HalfLength, HalfLength);

	const float HalfWidth = ChunkSize.Y * 0.5f;
	const float MinY = FMath::Clamp(FMath::Min(Definition.LateralRange.X, Definition.LateralRange.Y), -HalfWidth, HalfWidth);
	const float MaxY = FMath::Clamp(FMath::Max(Definition.LateralRange.X, Definition.LateralRange.Y), MinY, HalfWidth);
	const float MinYaw = FMath::Min(Definition.YawRange.X, Definition.YawRange.Y);
	const float MaxYaw = FMath::Max(Definition.YawRange.X, Definition.YawRange.Y);
	const float MinScale = FMath::Max(0.001f, FMath::Min(Definition.UniformScaleRange.X, Definition.UniformScaleRange.Y));
	const float MaxScale = FMath::Max(MinScale, FMath::Max(Definition.UniformScaleRange.X, Definition.UniformScaleRange.Y));

	float LocalY = Stream.FRandRange(MinY, MaxY);
	if (bExcludeRailAreaFromRandomPlacement && Definition.PlacementMode == ETRLevelScenePlacementMode::Random)
	{
		const float ExclusionMin = FMath::Min(RailLateralExclusionRange.X, RailLateralExclusionRange.Y);
		const float ExclusionMax = FMath::Max(RailLateralExclusionRange.X, RailLateralExclusionRange.Y);

		if (FMath::IsNearlyEqual(MinY, MaxY))
		{
			if (LocalY >= ExclusionMin && LocalY <= ExclusionMax)
			{
				return false;
			}
		}
		else
		{
			const float OverlapMin = FMath::Max(MinY, ExclusionMin);
			const float OverlapMax = FMath::Min(MaxY, ExclusionMax);
			if (OverlapMin < OverlapMax)
			{
				const float LeftWidth = FMath::Max(0.f, OverlapMin - MinY);
				const float RightWidth = FMath::Max(0.f, MaxY - OverlapMax);
				const float TotalAllowedWidth = LeftWidth + RightWidth;
				if (TotalAllowedWidth <= KINDA_SMALL_NUMBER)
				{
					return false;
				}

				if (Stream.FRandRange(0.f, TotalAllowedWidth) < LeftWidth)
				{
					LocalY = Stream.FRandRange(MinY, OverlapMin);
				}
				else
				{
					LocalY = Stream.FRandRange(OverlapMax, MaxY);
				}
			}
		}
	}

	const FVector Location(LocalX, LocalY, 0.f);
	const FRotator Rotation(0.f, Stream.FRandRange(MinYaw, MaxYaw), 0.f);
	const float UniformScale = Stream.FRandRange(MinScale, MaxScale);
	OutTransform = FTransform(Rotation, Location, FVector(UniformScale));
	return true;
}

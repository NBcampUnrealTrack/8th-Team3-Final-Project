// TRLevelSceneChunk.h

#pragma once

#include "CoreMinimal.h"
#include "Environment/TRLevelSceneTypes.h"
#include "GameFramework/Actor.h"
#include "TRLevelSceneChunk.generated.h"

class UInstancedStaticMeshComponent;
class USceneComponent;

/**
 * 레벨 환경 청크 단위 액터
 * ISM을 이용해 다량의 메시를 하나의 액터에서 관리
 */

UCLASS(Blueprintable)
class TR_API ATRLevelSceneChunk : public AActor
{
	GENERATED_BODY()

public:
	ATRLevelSceneChunk();

	void InitializeChunk(const TArray<FTRLevelSceneMeshDefinition>& InMeshDefinitions, const FVector2D& InChunkSize,
		bool bInExcludeRailAreaFromRandomPlacement, const FVector2D& InRailLateralExclusionRange);
	void GenerateChunk(int32 InChunkSeed, int32 InSegmentIndex);

	int32 GetSegmentIndex() const 
	{
		return SegmentIndex;
	}
	FVector2D GetChunkSize() const 
	{
		return ChunkSize;
	}

private:
	void RebuildInstanceComponents();
	void ClearInstances();
	bool CreateInstanceTransform(const FTRLevelSceneMeshDefinition& Definition, int32 DefinitionIndex,
		int32 InstanceIndex, int32 InstanceCount, int32 ChunkSeed, FTransform& OutTransform) const;

	UPROPERTY(VisibleAnywhere, Category = "Level Scene")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> SceneInstanceComponents;

	UPROPERTY(Transient)
	TArray<FTRLevelSceneMeshDefinition> MeshDefinitions;

	FVector2D ChunkSize = FVector2D(10000.f, 20000.f);
	bool bExcludeRailAreaFromRandomPlacement = true;
	FVector2D RailLateralExclusionRange = FVector2D::ZeroVector;
	int32 SegmentIndex = INDEX_NONE;
};

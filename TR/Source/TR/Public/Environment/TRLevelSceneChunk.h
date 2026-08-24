// TRLevelSceneChunk.h

#pragma once

#include "CoreMinimal.h"
#include "Environment/TRLevelSceneTypes.h"
#include "GameFramework/Actor.h"
#include "TRTypes.h"
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

	/**
	 * 소품 세트를 갈아 끼운다. **같은 바이옴이면 아무것도 하지 않는다.**
	 *
	 * 세트를 바꾸면 ISM 컴포넌트를 전부 다시 만들어야 해서(메시가 달라지므로) 재활용
	 * 때마다 부르면 비싸다. 그래서 바이옴 번호로 먼저 걸러낸다 — 실제 재구성은 전환이
	 * 진행되는 동안 청크 하나당 한 번씩만 일어난다.
	 *
	 * 부른 뒤에는 GenerateChunk 로 배치를 다시 만들어야 한다. 이 함수는 그릇만 바꾼다.
	 */
	void ApplyBiome(ETRBiomeType InBiome, const TArray<FTRLevelSceneMeshDefinition>& InMeshDefinitions);

	/** 지금 담고 있는 풍경. 아직 지정된 적 없으면 None. */
	ETRBiomeType GetBiome() const
	{
		return Biome;
	}

	/**
	 * 지금 이 청크에 실제로 박혀 있는 소품 개수.
	 *
	 * 설정을 아무리 들여다봐도 "그래서 몇 개가 나왔나"는 알 수 없다. 정의는 채워져
	 * 있는데 개수가 0 이거나 배치가 전부 제외 구간에 걸려 튕긴 경우, 겉보기에는
	 * 그냥 아무것도 없는 벌판이라 원인을 못 짚는다.
	 */
	int32 GetTotalInstanceCount() const;

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

	/**
	 * 지금 담고 있는 풍경. ApplyBiome 이 같은 값을 걸러내는 근거다.
	 *
	 * MAX 로 시작하는 이유는 None 도 유효한 값이기 때문이다. 기본값이 None 이면
	 * "바이옴을 안 쓰는 레벨"의 첫 ApplyBiome(None) 이 걸러져 소품이 안 생긴다.
	 */
	ETRBiomeType Biome = ETRBiomeType::MAX;
};

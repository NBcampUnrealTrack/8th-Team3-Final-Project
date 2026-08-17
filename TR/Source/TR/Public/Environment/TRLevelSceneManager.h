// TRLevelSceneManager.h

#pragma once

#include "CoreMinimal.h"
#include "Environment/TRLevelSceneTypes.h"
#include "GameFramework/Actor.h"
#include "TRLevelSceneManager.generated.h"

class ATRLevelSceneChunk;
class ATRLevelSceneFloor;
class ATRLevelSceneRailSegment;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;

/**
 * 레벨 환경 자동생성 관리자
 * 할당된 메시들을 규칙에 따라 랜덤배치하며 이동시켜 속도감, 자연스러움 구현
 * ISM 활용하여 제한적인 메시로 재활용하면서 반복 사용
 */

UCLASS(Blueprintable)
class TR_API ATRLevelSceneManager : public AActor
{
	GENERATED_BODY()

public:
	ATRLevelSceneManager();

	virtual void Tick(float DeltaSeconds) override;

	//---변수 Setter---
	UFUNCTION(BlueprintCallable, Category = "Level Scene")
	void SetVisualSpeed(float NewVisualSpeed);

	UFUNCTION(BlueprintCallable, Category = "Level Scene")
	void SetRouteSeed(int32 NewRouteSeed, bool bRegenerate = true);

	UFUNCTION(BlueprintCallable, Category = "Level Scene")
	void RegenerateAllChunks();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	//--- 레벨 초기 설정 함수 --- 
	void CreateInitialChunks();
	void CreateInitialRailSegments();
	void CreateFloor();
	void ValidateGenerationSettings() const;

	//--- 이동 효과 관련 함수 ---
	//지금 배경을 흘려도 되는가. bAutoMove 와 판 시작 여부를 함께 본다.
	bool IsSceneMovementAllowed() const;
	void MoveChunks(float DeltaSeconds);
	void MoveRailSegments(float DeltaSeconds);
	void RecyclePassedChunks();
	void RecyclePassedRailSegments();
	void RecycleChunk(ATRLevelSceneChunk* Chunk);
	void RecycleRailSegment(ATRLevelSceneRailSegment* RailSegment);
	void UpdateFloorVelocity(bool bForceUpdate = false);
	float GetTravelDistance(const ATRLevelSceneChunk* Chunk) const;
	float GetRailTravelDistance(const ATRLevelSceneRailSegment* RailSegment) const;
	float GetFrontmostTravelDistance(const ATRLevelSceneChunk* IgnoredChunk = nullptr) const;
	float GetFrontmostRailTravelDistance(const ATRLevelSceneRailSegment* IgnoredRailSegment = nullptr) const;

	//랜덤 설정/주입 함수
	int32 MakeChunkSeed(int32 SegmentIndex) const;

	//--- 메시 관리용 정보 ---
	UPROPERTY(VisibleAnywhere, Category = "Level Scene")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, Category = "Level Scene|Generation")
	TSubclassOf<ATRLevelSceneChunk> ChunkClass;

	UPROPERTY(EditAnywhere, Category = "Level Scene|Generation")
	TArray<FTRLevelSceneMeshDefinition> MeshDefinitions;

	//--- 메시 생성/업데이트용 수치 ---
	//배치에 사용될 난수열 생성용 시드값
	UPROPERTY(EditAnywhere, Category = "Level Scene|Generation")
	int32 RouteSeed = 1337;

	//반복에 사용할 청크 액터 갯수
	UPROPERTY(EditAnywhere, Category = "Level Scene|Chunk", meta = (ClampMin = "1"))
	int32 ChunkCount = 10;

	//청크 액터의 가로새로 평면 사이즈
	UPROPERTY(EditAnywhere, Category = "Level Scene|Chunk", meta = (ClampMin = "1.0"))
	FVector2D ChunkSize = FVector2D(3000.f, 30000.f);

	//시작시 뒤쪽으로 미리 생성할 청크 갯수
	UPROPERTY(EditAnywhere, Category = "Level Scene|Chunk", meta = (ClampMin = "0"))
	int32 InitialBehindChunkCount = 5;

	//청크가 뒤로 멀어졌을 때 회수할 기준값, 더 멀어지면 앞쪽으로 초기화하여 재배치
	UPROPERTY(EditAnywhere, Category = "Level Scene|Chunk", meta = (ClampMin = "0.0"))
	float RecycleBehindDistance = 15000.f;

	//이동속도, 기차의 이동속도와 연동하여 속도감 동기화
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float VisualSpeed = 1000.f;

	//자동 이동 여부 설정
	UPROPERTY(EditAnywhere, Category = "Level Scene|Movement")
	bool bAutoMove = true;

	//판이 시작된 뒤에만 움직일지 여부. 끄면 로비에서도 배경이 흐른다.
	UPROPERTY(EditAnywhere, Category = "Level Scene|Movement")
	bool bMoveOnlyAfterMatchStart = true;

	//레일 생성 여부 결정
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail")
	bool bGenerateRail = true;

	//레일 메시 생성용 액터 클래스
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail")
	TSubclassOf<ATRLevelSceneRailSegment> RailSegmentClass;
	
	//레일 메시
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail")
	TObjectPtr<UStaticMesh> RailMesh;

	//생성할 레일 갯수
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail", meta = (ClampMin = "1"))
	int32 RailSegmentCount = 24;

	//시작시 뒤쪽으로 미리 생성할 레일 조각 갯수
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail", meta = (ClampMin = "0"))
	int32 InitialBehindRailSegmentCount = 15;

	//레일 배치 간격
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail", meta = (ClampMin = "1.0"))
	float RailSegmentLength = 1000.f;

	//레일이 뒤로 멀어졌을 때 회수할 기준값, 더 멀어지면 앞쪽으로 재배치
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail", meta = (ClampMin = "0.0"))
	float RailRecycleBehindDistance = 15000.f;

	//레일 배치 오프셋
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail")
	FVector RailOffset = FVector::ZeroVector;

	//레일 주변 위치로 인식할 범위. 레일이 생성될 때 이 폭 안에는 Random 배치하지 않음.
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail|Placement", meta = (ClampMin = "0.0", ToolTip = "Half-width of the no-placement strip centered on RailOffset.Y."))
	float RailPlacementClearanceHalfWidth = 750.f;

	//레일 액터 컬링 거리
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail", meta = (ClampMin = "0"))
	int32 RailEndCullDistance = 30000;

	//레일 그림자 생성 여부
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail")
	bool bRailCastShadow = true;

	//지면 사용할지 여부
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	bool bGenerateFloor = true;

	//기존 레벨에 있는 지면 사용할지, 혹은 새로 생성할지 여부
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	bool bUseExistingLevelFloor = true;

	//이미 배치되어있는 사용할 지면 액터
	UPROPERTY(EditInstanceOnly, Category = "Level Scene|Floor", meta = (EditCondition = "bUseExistingLevelFloor"))
	TObjectPtr<AActor> ExistingFloorActor;

	//지면 액터 클래스
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	TSubclassOf<ATRLevelSceneFloor> FloorClass;

	//지면 액터에 사용할 메시
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	TObjectPtr<UStaticMesh> FloorMesh;

	//지면 메시에 사용할 머티리얼
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	TObjectPtr<UMaterialInterface> FloorMaterial;

	//지면 액터 생성시 위치
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	FTransform FloorRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -20.f), FVector(200.f, 200.f, 1.f));

	//지면 머티리얼에 적용할 Panner이동효과 방향
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	FVector2D FloorMovementDirection = FVector2D(1.f, 0.f);

	//---관리 대상 액터---
	//레벨에 배치된 청크 액터들
	UPROPERTY(Transient)
	TArray<TObjectPtr<ATRLevelSceneChunk>> ActiveChunks;

	//레벨에 배치된 레일 액터들
	UPROPERTY(Transient)
	TArray<TObjectPtr<ATRLevelSceneRailSegment>> ActiveRailSegments;

	//레벨에 배치된 지면 액터
	UPROPERTY(Transient)
	TObjectPtr<ATRLevelSceneFloor> ActiveFloor;

	//속도 변경시 이전값 저장용도
	float LastAppliedFloorVisualSpeed = -1.f;
	FVector2D LastAppliedFloorMovementDirection = FVector2D::ZeroVector;

	//청크 재활용시 재생성용 인덱스
	int32 NextSegmentIndex = 0;
};

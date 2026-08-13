// TRLevelSceneTypes.h

#pragma once

#include "CoreMinimal.h"
#include "TRLevelSceneTypes.generated.h"

class UStaticMesh;
class UMaterialInterface;

// 레벨 메시 배치 방식 - 랜덤하게 초기화, 혹은 반복시 X좌표 일정하게 배치. 필요시 LongitudinalJitter 활용해 오차 추가 가능.
UENUM(BlueprintType)
enum class ETRLevelScenePlacementMode : uint8
{
	Random,
	UniformAlongLength
};

// 레벨 메시 배치 정보
USTRUCT(BlueprintType)
struct TR_API FTRLevelSceneMeshDefinition
{
	GENERATED_BODY()

	//메시 에셋
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	//메시 Material Slot별 Override. 비워 두거나 비어 있는 항목은 Static Mesh 기본 머티리얼을 사용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	//배치 방식
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene")
	ETRLevelScenePlacementMode PlacementMode = ETRLevelScenePlacementMode::Random;

	//배치 최저 갯수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene", meta = (ClampMin = "0"))
	int32 MinInstanceCount = 0;

	//배치 최대 갯수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene", meta = (ClampMin = "0"))
	int32 MaxInstanceCount = 0;

	//좌우 배치 너비, 얼마나 멀리까지 배치할지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene")
	FVector2D LateralRange = FVector2D(-15000.f, 15000.f);

	//이 Definition이 생성되지 않을 로컬 Y축 구간 목록. 각 원소의 X=최소 Y, Y=최대 Y.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene")
	TArray<FVector2D> LateralExclusionRanges;

	//회전값 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene")
	FVector2D YawRange = FVector2D(0.f, 360.f);

	//랜덤 크기 설정, 기본값은 크기는 고정시킴
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene", meta = (ClampMin = "0.001"))
	FVector2D UniformScaleRange = FVector2D(1.f, 1.f);

	//X축 기준 앞뒤 오차 정도, UniformAlongLength시 반복성 줄이려면 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene", meta = (ClampMin = "0.0"))
	float LongitudinalJitter = 0.f;

	//컬링 시작 거리, 기본값으론 적용 안함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene", meta = (ClampMin = "0"))
	int32 StartCullDistance = 0;
	
	//컬링 종료 지점, 더 멀어지면 렌더링 안함. 기본값으론 사용 안함.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene", meta = (ClampMin = "0"))
	int32 EndCullDistance = 0;

	//그림자 생성 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene")
	bool bCastShadow = true;
};

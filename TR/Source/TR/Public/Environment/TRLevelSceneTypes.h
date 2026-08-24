// TRLevelSceneTypes.h

#pragma once

#include "CoreMinimal.h"
#include "TRLevelSceneTypes.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UTexture;
class UTRBiomeEnvironment;

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

/**
 * 배치 정의 배열을 담는 껍데기.
 *
 * TMap 의 값으로 배열을 직접 넣을 수 없어서 한 겹 씌운 것이다. 그 이상의 뜻은 없다.
 */
USTRUCT()
struct TR_API FTRLevelSceneMeshDefinitionSet
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTRLevelSceneMeshDefinition> Definitions;
};

/**
 * 한 구간의 풍경. 종류(ETRBiomeType)가 열쇠이고, 이 구조체는 그 종류가 실제로
 * 어떤 소품과 지면인지를 정의한다. 언제 바뀔지는 웨이브 테이블이 정한다.
 *
 * **전환은 청크 재활용을 타고 저절로 일어난다.** 기차는 고정이고 지형이 흘러오므로,
 * 새 바이옴의 소품은 화면 앞쪽에서 생성되어 시간을 두고 다가온다. 로딩도 순간이동도
 * 없이 "달려서 다른 곳에 도착했다"가 되는 것이 이 구조의 이점이다.
 *
 * 지면 텍스처와 날씨(눈/젖음)까지 여기서 정의한다. 조명은 아직 없다 — 필요해지면
 * 같은 구조체에 덧붙일 자리다.
 */
USTRUCT(BlueprintType)
struct TR_API FTRBiomeDefinition
{
	GENERATED_BODY()

	/**
	 * 이 바이옴 **전용** 소품. 매니저의 SharedMeshDefinitions 에 얹혀서 함께 배치된다.
	 *
	 * 비워 두면 공통 소품만 깔린다. 공통까지 없애서 허허벌판을 만들고 싶으면
	 * bIgnoreSharedMeshes 를 켜고 이 배열도 비운다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome")
	TArray<FTRLevelSceneMeshDefinition> MeshDefinitions;

	/**
	 * 공통 소품을 빼고 이 바이옴 전용만 쓴다.
	 *
	 * **허허벌판을 일부러 만들 때 필요하다.** 이 스위치가 없으면 "공통을 안 쓰겠다"와
	 * "아직 안 채웠다"가 둘 다 빈 배열로 보여서 구별이 안 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome")
	bool bIgnoreSharedMeshes = false;

	/**
	 * 지면 텍스처. **둘 다 비워 두면 이 바이옴에서는 지면을 건드리지 않는다.**
	 *
	 * 소품만 바꾸고 지면은 그대로 두고 싶은 구간(같은 사막 안에서 바위 종류만 바뀌는 식)이
	 * 있을 수 있어서, 비워 두는 것을 "지면 유지"로 해석한다. 지우는 뜻이 아니다.
	 *
	 * 지면 머테리얼은 M_TR_FloorBiome 처럼 TextureBase/TextureNormal 과
	 * TextureBaseB/TextureNormalB, BiomeBlend 를 갖고 있어야 한다. 그 이름들이
	 * ATRLevelSceneFloor 의 파라미터 이름 설정과 맞아야 값이 들어간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Floor")
	TObjectPtr<UTexture> FloorBaseColor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Floor")
	TObjectPtr<UTexture> FloorNormal = nullptr;

	/**
	 * 이 바이옴에서 지면에 쌓이는 눈의 양(0~1).
	 *
	 * 텍스처를 바꾸는 것과 **성격이 다르다.** 텍스처는 "다른 땅에 도착했다"이고,
	 * 이 값은 "여기 날씨가 이렇다"다. 그래서 경계선을 타고 오는 것이 아니라
	 * 도착한 뒤 SurfaceAccumulationSeconds 동안 서서히 차오른다.
	 *
	 * 0.3 쯤이면 낮은 곳에만 희끗하게, 1 이면 온통 하얗게 덮인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Surface", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SnowAmount = 0.f;

	/**
	 * 지면이 젖은 정도(0~1). 값이 커질수록 전체가 어두워지고 반사가 세지며,
	 * 웅덩이가 넓어진다. 비 오는 구간을 표현할 때 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Surface", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Wetness = 0.f;

	/**
	 * 위 두 값이 0 에서 목표치까지 차오르는 데 걸리는 시간(초). 0 이면 즉시.
	 *
	 * **경계선 전환보다 느리게 두는 것이 자연스럽다.** 땅은 선을 그으며 바뀌지만
	 * 눈은 쌓이는 데 시간이 걸리고 물은 고이는 데 시간이 걸린다. 둘이 같은 속도로
	 * 오면 눈이 "쌓인" 것이 아니라 "칠해진" 것으로 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Surface", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SurfaceAccumulationSeconds = 8.f;

	/**
	 * 이 풍경의 안개·해·하늘빛·포스트프로세스.
	 *
	 * **비워 두면 환경을 건드리지 않는다.** 레벨에 놓인 값이 그대로 유지된다. 지우는
	 * 뜻이 아니다 — 아직 안 채운 바이옴 때문에 안개가 사라지고 해가 꺼지면, 설정을
	 * 빠뜨린 것이 사고처럼 보인다.
	 *
	 * 값 자체는 레벨이 아니라 애셋에 있다. 그래야 두 사람이 서로 다른 바이옴을 동시에
	 * 손봐도 .umap 에서 충돌하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Environment")
	TObjectPtr<UTRBiomeEnvironment> Environment = nullptr;

	/** 지면 텍스처가 하나라도 지정돼 있는지. 없으면 지면 전환을 건너뛴다. */
	bool HasFloorTextures() const
	{
		return FloorBaseColor != nullptr || FloorNormal != nullptr;
	}

	/** 날씨 효과값. X=눈, Y=젖음. 두 값을 늘 함께 다루므로 한 벡터로 묶는다. */
	FVector2D GetSurfaceEffect() const
	{
		return FVector2D(SnowAmount, Wetness);
	}
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRLevelSceneFloor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 지면 머테리얼의 한 슬롯에 해당하는 파라미터 이름 묶음.
 *
 * 지면 머테리얼은 바이옴 두 벌을 동시에 샘플해서 경계선으로 가른다. 그래서 모든
 * 파라미터가 A/B 로 짝을 이루고, **네 이름은 언제나 함께 움직인다** — 슬롯 하나를
 * 고르면 텍스처와 날씨가 통째로 따라온다. 낱개 FName 12 개로 두면 호출부마다
 * `bSlotB ? XxxB : XxxA` 를 네 번 쓰게 되고, 한 줄만 짝이 어긋나도 눈에 띄지 않는다.
 *
 * 비워 두면(None) 그 파라미터는 쓰지 않는 것으로 보고 검사에서도 빠진다.
 */
USTRUCT(BlueprintType)
struct TR_API FTRFloorBiomeSlotParameters
{
	GENERATED_BODY()

	FTRFloorBiomeSlotParameters() = default;

	/**
	 * 기본 이름 뒤에 접미사를 붙여 만든다. B 슬롯은 A 와 같은 이름에 "B" 만 붙는다는
	 * 규약을 코드로 적어 두는 것이다.
	 */
	explicit FTRFloorBiomeSlotParameters(const TCHAR* Suffix)
		: Base(*FString::Printf(TEXT("TextureBase%s"), Suffix))
		, Normal(*FString::Printf(TEXT("TextureNormal%s"), Suffix))
		, SnowAmount(*FString::Printf(TEXT("SnowAmount%s"), Suffix))
		, Wetness(*FString::Printf(TEXT("Wetness%s"), Suffix))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor|Biome")
	FName Base = TEXT("TextureBase");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor|Biome")
	FName Normal = TEXT("TextureNormal");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor|Biome")
	FName SnowAmount = TEXT("SnowAmount");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor|Biome")
	FName Wetness = TEXT("Wetness");
};

/**
 * 지면 액터
 * 지면 머티리얼 상호작용 기능 제공
 */
UCLASS(Blueprintable)
class TR_API ATRLevelSceneFloor : public AActor
{
	GENERATED_BODY()

public:
	ATRLevelSceneFloor();

	void InitializeFloor(UStaticMesh* InFloorMesh, UMaterialInterface* InFloorMaterial);
	void InitializeExistingFloor(UStaticMeshComponent* InFloorMeshComponent);

	UFUNCTION(BlueprintCallable, Category = "Level Scene|Floor")
	void SetMovementVelocity(float MovementVelocityX, float MovementVelocityY);

	/**
	 * 바이옴 텍스처를 A 또는 B 슬롯에 넣는다.
	 *
	 * 지면 머테리얼은 두 벌을 동시에 샘플해서 BiomeBlend 로 섞는다. 그래서 전환 중에는
	 * 두 슬롯이 서로 다른 바이옴을 들고 있어야 하고, 전환이 끝나면 B 를 A 로 옮겨
	 * 다음 전환을 위해 B 를 비워 둔다(핑퐁).
	 *
	 * @param bSlotB true 면 다음 바이옴(B), false 면 현재 바이옴(A).
	 * @param Base   nullptr 이면 그 파라미터는 건드리지 않는다. 지우는 뜻이 아니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Scene|Floor")
	void SetBiomeTextures(UTexture* Base, UTexture* Normal, bool bSlotB);

	/**
	 * 바이옴 경계를 지정한다.
	 *
	 * 화면 전체를 섞지 않고 **월드 공간에 선을 하나 긋는다.** 그 선보다 진행 방향
	 * 앞쪽은 다음 풍경(B), 뒤쪽은 현재 풍경(A)이다. 선을 기차 쪽으로 옮기면 새 지형이
	 * 밀려 들어온다.
	 *
	 * 전체 Lerp 를 쓰지 않는 이유는 두 텍스처가 겹친 중간색이 화면을 덮기 때문이다.
	 * 그건 "다른 지형에 도착했다"가 아니라 "색이 탁해졌다"로 보인다.
	 *
	 * @param Boundary 진행 축에 투영한 월드 좌표(cm). 크게 주면 화면 밖(= 전환 없음).
	 * @param Axis     진행 방향. 안에서 정규화하므로 길이는 아무래도 좋다.
	 * @param Width    경계가 흐려지는 폭(cm). 0 에 가까우면 칼같이 잘린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Scene|Floor")
	void SetBiomeBoundary(float Boundary, FVector2D Axis, float Width);

	/**
	 * 날씨 효과(눈/젖음)를 A 또는 B 슬롯에 넣는다.
	 *
	 * 텍스처와 같은 경계 마스크로 섞이므로, 전환 중에는 경계 앞뒤로 다른 날씨가 보인다.
	 * 눈밭 경계선 너머에만 눈이 얹혀 있는 그림이 나온다.
	 *
	 * @param SnowAmount 0~1. 지면에 쌓인 눈의 양.
	 * @param Wetness    0~1. 젖은 정도. 커질수록 어두워지고 웅덩이가 넓어진다.
	 * @param bSlotB     true 면 다음 바이옴(B), false 면 현재 바이옴(A).
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Scene|Floor")
	void SetBiomeSurfaceEffect(float SnowAmount, float Wetness, bool bSlotB);

private:
	/**
	 * 쓸 수 있는 다이나믹 머테리얼을 돌려준다. 없으면 그 자리에서 만든다.
	 *
	 * 파라미터를 건드리는 모든 함수가 이걸 먼저 부른다. 지면 메시가 아직 안 붙었을 때는
	 * nullptr 이므로, 호출부는 결과를 반드시 확인해야 한다.
	 */
	UMaterialInstanceDynamic* GetOrCreateMaterial();

	void RebuildDynamicMaterial();

	/**
	 * 설정된 파라미터 이름이 실제로 머테리얼에 있는지 한 번 확인한다.
	 *
	 * **이름이 틀리면 SetXxxParameterValue 는 조용히 아무것도 하지 않는다.** 에러도
	 * 경고도 없이 지면만 안 바뀌므로, 머테리얼을 갈아 끼운 뒤 오타 하나로 하루를 쓰기
	 * 좋다. 머테리얼을 새로 만들 때 한 번만 도는 검사라 비용은 없다.
	 */
	void ValidateMaterialParameters() const;

	/** 슬롯 하나에 해당하는 이름 묶음. */
	const FTRFloorBiomeSlotParameters& GetSlot(const bool bSlotB) const
	{
		return bSlotB ? BiomeSlotB : BiomeSlotA;
	}

	UPROPERTY(VisibleAnywhere, Category = "Level Scene|Floor")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Level Scene|Floor")
	TObjectPtr<UStaticMeshComponent> FloorMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ControlledFloorMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FloorMaterialInstance;

	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	FName MovementVelocityXParameter = TEXT("MovementVelocityX");

	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	FName MovementVelocityYParameter = TEXT("MovementVelocityY");

	// 아래 이름들은 지면 머테리얼의 파라미터 이름과 정확히 같아야 한다.
	// 틀린 이름은 ValidateMaterialParameters 가 머테리얼 생성 시점에 경고로 잡아 준다.

	/** 지금 보이는 풍경(A). 전환이 없을 때 화면 전체가 이 슬롯이다. */
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor|Biome")
	FTRFloorBiomeSlotParameters BiomeSlotA;

	/** 경계 너머에서 밀려오는 다음 풍경(B). 전환이 끝나면 A 로 옮겨진다. */
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor|Biome")
	FTRFloorBiomeSlotParameters BiomeSlotB = FTRFloorBiomeSlotParameters(TEXT("B"));

	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor|Biome")
	FName BiomeBoundaryParameter = TEXT("BiomeBoundary");

	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor|Biome")
	FName BiomeAxisXParameter = TEXT("BiomeAxisX");

	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor|Biome")
	FName BiomeAxisYParameter = TEXT("BiomeAxisY");

	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor|Biome")
	FName BiomeBlendWidthParameter = TEXT("BiomeBlendWidth");
};

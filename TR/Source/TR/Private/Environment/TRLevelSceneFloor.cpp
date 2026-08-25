#include "Environment/TRLevelSceneFloor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/TRGameplayLog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameters.h"
#include "Engine/Texture.h"

//지면 액터 생성자
ATRLevelSceneFloor::ATRLevelSceneFloor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FloorMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMesh"));
	FloorMeshComponent->SetupAttachment(SceneRoot);
	FloorMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FloorMeshComponent->SetGenerateOverlapEvents(false);
	FloorMeshComponent->SetCanEverAffectNavigation(false);
}

//지면 액터 정보 초기화
void ATRLevelSceneFloor::InitializeFloor(UStaticMesh* InFloorMesh, UMaterialInterface* InFloorMaterial)
{
	ControlledFloorMeshComponent = FloorMeshComponent;
	FloorMeshComponent->SetStaticMesh(InFloorMesh);
	if (InFloorMaterial)
	{
		FloorMeshComponent->SetMaterial(0, InFloorMaterial);
	}
	RebuildDynamicMaterial();
}

//이미 있는 지면액터 기반으로 정보 초기화
void ATRLevelSceneFloor::InitializeExistingFloor(UStaticMeshComponent* InFloorMeshComponent)
{
	ControlledFloorMeshComponent = InFloorMeshComponent;
	RebuildDynamicMaterial();
}

//이동속도 파라미터 업데이트
void ATRLevelSceneFloor::SetMovementVelocity(const float MovementVelocityX, const float MovementVelocityY)
{
	UMaterialInstanceDynamic* Material = GetOrCreateMaterial();
	if (Material == nullptr)
	{
		return;
	}

	Material->SetScalarParameterValue(MovementVelocityXParameter, MovementVelocityX);
	Material->SetScalarParameterValue(MovementVelocityYParameter, MovementVelocityY);
}

//바이옴 텍스처를 A/B 슬롯에 넣는다
void ATRLevelSceneFloor::SetBiomeTextures(UTexture* Base, UTexture* Normal, const bool bSlotB)
{
	UMaterialInstanceDynamic* Material = GetOrCreateMaterial();
	if (Material == nullptr)
	{
		return;
	}

	const FTRFloorBiomeSlotParameters& Slot = GetSlot(bSlotB);

	// nullptr 은 "건드리지 않는다"로 본다. 지우는 뜻으로 쓰면 텍스처가 없는 바이옴에서
	// 지면이 검게 나오는데, 그건 연출이 아니라 사고로 보인다.
	if (Base)
	{
		Material->SetTextureParameterValue(Slot.Base, Base);
	}

	if (Normal)
	{
		Material->SetTextureParameterValue(Slot.Normal, Normal);
	}
}

//바이옴 경계
void ATRLevelSceneFloor::SetBiomeBoundary(const float Boundary, const FVector2D Axis, const float Width)
{
	UMaterialInstanceDynamic* Material = GetOrCreateMaterial();
	if (Material == nullptr)
	{
		return;
	}

	// 블루프린트에서도 부를 수 있는 함수라 축을 여기서 한 번 더 정규화한다. 길이가
	// 1 이 아니면 경계가 엉뚱한 자리에 그어지는데, 화면만 보고는 원인을 알기 어렵다.
	const FVector2D SafeAxis = Axis.IsNearlyZero() ? FVector2D(1.f, 0.f) : Axis.GetSafeNormal();

	Material->SetScalarParameterValue(BiomeBoundaryParameter, Boundary);
	Material->SetScalarParameterValue(BiomeAxisXParameter, static_cast<float>(SafeAxis.X));
	Material->SetScalarParameterValue(BiomeAxisYParameter, static_cast<float>(SafeAxis.Y));

	// 0 으로 나누면 경계 전체가 NaN 이 되어 지면이 통째로 사라진다.
	Material->SetScalarParameterValue(BiomeBlendWidthParameter, FMath::Max(Width, 1.f));
}

//날씨 효과를 A/B 슬롯에 넣는다
void ATRLevelSceneFloor::SetBiomeSurfaceEffect(const float SnowAmount, const float Wetness, const bool bSlotB)
{
	UMaterialInstanceDynamic* Material = GetOrCreateMaterial();
	if (Material == nullptr)
	{
		return;
	}

	const FTRFloorBiomeSlotParameters& Slot = GetSlot(bSlotB);

	// 텍스처와 달리 0 이 유효한 값이다("눈 없음"). 그래서 여기서는 nullptr 같은
	// "건드리지 않는다" 규칙이 없고, 받은 값을 늘 그대로 밀어 넣는다.
	Material->SetScalarParameterValue(Slot.SnowAmount, FMath::Clamp(SnowAmount, 0.f, 1.f));
	Material->SetScalarParameterValue(Slot.Wetness, FMath::Clamp(Wetness, 0.f, 1.f));
}

//쓸 수 있는 다이나믹 머티리얼을 얻는다. 없으면 만든다
UMaterialInstanceDynamic* ATRLevelSceneFloor::GetOrCreateMaterial()
{
	if (!FloorMaterialInstance)
	{
		RebuildDynamicMaterial();
	}

	return FloorMaterialInstance;
}

//다이나믹 머티리얼 재설정
void ATRLevelSceneFloor::RebuildDynamicMaterial()
{
	FloorMaterialInstance = ControlledFloorMeshComponent
		? ControlledFloorMeshComponent->CreateAndSetMaterialInstanceDynamic(0)
		: nullptr;

	ValidateMaterialParameters();
}

//설정된 파라미터 이름이 머티리얼에 실제로 있는지 확인한다
void ATRLevelSceneFloor::ValidateMaterialParameters() const
{
	if (!FloorMaterialInstance)
	{
		return;
	}

	const auto ReportMissing = [this](const EMaterialParameterType Type, const FName ParameterName)
	{
		// 비워 둔 이름은 "이 파라미터는 안 쓴다"는 뜻이다. 없다고 나무랄 일이 아니다.
		if (ParameterName.IsNone())
		{
			return;
		}

		FMaterialParameterMetadata Unused;
		if (!FloorMaterialInstance->GetParameterValue(Type, ParameterName, Unused))
		{
			UE_LOG(LogTRGameplay, Warning,
				TEXT("[LevelScene] 지면 머테리얼 %s 에 파라미터 '%s' 가 없다. 이 값은 무시된다 — %s 의 이름 설정을 확인할 것."),
				*GetNameSafe(FloorMaterialInstance->Parent), *ParameterName.ToString(), *GetName());
		}
	};

	const auto ReportMissingSlot = [&ReportMissing](const FTRFloorBiomeSlotParameters& Slot)
	{
		ReportMissing(EMaterialParameterType::Texture, Slot.Base);
		ReportMissing(EMaterialParameterType::Texture, Slot.Normal);
		ReportMissing(EMaterialParameterType::Scalar, Slot.SnowAmount);
		ReportMissing(EMaterialParameterType::Scalar, Slot.Wetness);
	};

	ReportMissingSlot(BiomeSlotA);
	ReportMissingSlot(BiomeSlotB);

	ReportMissing(EMaterialParameterType::Scalar, MovementVelocityXParameter);
	ReportMissing(EMaterialParameterType::Scalar, MovementVelocityYParameter);
	ReportMissing(EMaterialParameterType::Scalar, BiomeBoundaryParameter);
	ReportMissing(EMaterialParameterType::Scalar, BiomeAxisXParameter);
	ReportMissing(EMaterialParameterType::Scalar, BiomeAxisYParameter);
	ReportMissing(EMaterialParameterType::Scalar, BiomeBlendWidthParameter);
}

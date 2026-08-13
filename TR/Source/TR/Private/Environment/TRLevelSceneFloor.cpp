#include "Environment/TRLevelSceneFloor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	if (!FloorMaterialInstance)
	{
		RebuildDynamicMaterial();
	}

	if (FloorMaterialInstance)
	{
		FloorMaterialInstance->SetScalarParameterValue(MovementVelocityXParameter, MovementVelocityX);
		FloorMaterialInstance->SetScalarParameterValue(MovementVelocityYParameter, MovementVelocityY);
	}
}

//다이나믹 머티리얼 재설정
void ATRLevelSceneFloor::RebuildDynamicMaterial()
{
	FloorMaterialInstance = ControlledFloorMeshComponent
		? ControlledFloorMeshComponent->CreateAndSetMaterialInstanceDynamic(0)
		: nullptr;
}

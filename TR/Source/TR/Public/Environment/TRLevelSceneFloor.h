#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRLevelSceneFloor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

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

private:
	void RebuildDynamicMaterial();

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
};

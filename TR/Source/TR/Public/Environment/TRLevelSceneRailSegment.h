#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRLevelSceneRailSegment.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 
 * 레일 조각 액터
 * 레일 메시 포함하여 레벨 씬 매니저에서 반복 재활용
 */
UCLASS(Blueprintable)
class TR_API ATRLevelSceneRailSegment : public AActor
{
	GENERATED_BODY()

public:
	ATRLevelSceneRailSegment();

	void InitializeRail(UStaticMesh* InRailMesh, int32 InEndCullDistance, bool bInCastShadow);

private:
	UPROPERTY(VisibleAnywhere, Category = "Level Scene|Rail")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Level Scene|Rail")
	TObjectPtr<UStaticMeshComponent> RailMeshComponent;
};

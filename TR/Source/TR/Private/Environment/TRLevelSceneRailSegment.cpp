#include "Environment/TRLevelSceneRailSegment.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

//레일 액터 생성자
ATRLevelSceneRailSegment::ATRLevelSceneRailSegment()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RailMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RailMesh"));
	RailMeshComponent->SetupAttachment(SceneRoot);
	RailMeshComponent->SetMobility(EComponentMobility::Movable);
	RailMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RailMeshComponent->SetGenerateOverlapEvents(false);
	RailMeshComponent->SetCanEverAffectNavigation(false);
}

//레일 액터 초기화
void ATRLevelSceneRailSegment::InitializeRail(UStaticMesh* InRailMesh, const int32 InEndCullDistance,
	const bool bInCastShadow)
{
	RailMeshComponent->SetStaticMesh(InRailMesh);
	RailMeshComponent->SetCullDistance(FMath::Max(0, InEndCullDistance));
	RailMeshComponent->SetCastShadow(bInCastShadow);
}

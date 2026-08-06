// TRInteractionComponent.cpp


#include "Interaction/TRInteractionComponent.h"
#include "Player/TRPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"

UTRInteractionComponent::UTRInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}


void UTRInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	
}


void UTRInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateTrace();
}

void UTRInteractionComponent::UpdateTrace()
{
	const ATRPlayerCharacter* OwnerCharacter = Cast<ATRPlayerCharacter>(GetOwner());
	UCameraComponent* CameraComp = OwnerCharacter ? OwnerCharacter->GetCameraComponent() : nullptr;
	if (!CameraComp)
	{
		LastTraceResult = nullptr;
		return;
	}
	
	UWorld* World = GetWorld();
	if (!World)
	{
		LastTraceResult = nullptr;
		return;
	}

	const FVector Start = CameraComp->GetComponentLocation();
	const FVector End = Start + CameraComp->GetForwardVector() * TraceDistance;

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);
	LastTraceResult = Hit.GetActor();
}


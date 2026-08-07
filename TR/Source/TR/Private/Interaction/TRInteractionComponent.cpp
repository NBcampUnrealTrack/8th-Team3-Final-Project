// TRInteractionComponent.cpp


#include "Interaction/TRInteractionComponent.h"
#include "Player/TRPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "UI/TRPlayerHUD.h"
#include "UI/TRPlayerHUDWidget.h"
#include "Train/TRInstallablePart.h"
#include "Train/TRSupplyPoint.h"
#include "GameFramework/PlayerController.h"

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
	UpdateInteractionWidget();
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

void UTRInteractionComponent::UpdateInteractionWidget()
{
	const APawn* OwnerPawn = GetOwner<APawn>();
	const APlayerController* PC = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	const ATRPlayerHUD* PlayerHUD = PC ? Cast<ATRPlayerHUD>(PC->GetHUD()) : nullptr;
	UTRPlayerHUDWidget* HUDWidget = PlayerHUD ? PlayerHUD->GetPlayerHUDWidget() : nullptr;
	if (!HUDWidget)
	{
		return;
	}

	// TODO: 상호작용 판정, 현재는 ATRInstallablePart과 ATRSupplyPoint만 체크.
	const AActor* Target = LastTraceResult.Get();
	HUDWidget->SetInteractionVisible(Target && (Target->IsA<ATRInstallablePart>() || Target->IsA<ATRSupplyPoint>()));
}


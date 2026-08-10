// TRInteractionComponent.cpp


#include "Interaction/TRInteractionComponent.h"
#include "Player/TRPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "UI/TRPlayerHUD.h"
#include "UI/TRPlayerHUDWidget.h"
#include "Interaction/TRTurretControlComponent.h"
#include "Interaction/TRActionComponent.h"
#include "Train/TRInstallablePart.h"
#include "Train/TRTrain.h"
#include "Train/TRTurret.h"
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
	const ATRPlayerCharacter* OwnerCharacter = Cast<ATRPlayerCharacter>(GetOwner());
	if (!IsValid(OwnerCharacter))
	{
		UE_LOG(LogTemp, Error, TEXT("Owner is not a valid APawn"));
		return;
	}

	const APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (!IsValid(PC))
	{
		return;
	}

	const ATRPlayerHUD* PlayerHUD = Cast<ATRPlayerHUD>(PC->GetHUD());
	if (!IsValid(PlayerHUD))
	{
		return;
	}

	UTRPlayerHUDWidget* HUDWidget = PlayerHUD->GetPlayerHUDWidget();
	if (!IsValid(HUDWidget))
	{
		return;
	}

	// 수리 진행 패널은 조준 대상과 무관하게 액션 상태만 따른다.
	const UTRActionComponent* ActionComp = OwnerCharacter->GetActionComponent();
	HUDWidget->SetRepairProgressVisible(IsValid(ActionComp) && ActionComp->GetCurrentActionType() == ETRActionType::Repair);

	// 터렛에 탑승 중이면 수리도 재장전도 못 하므로 프롬프트를 전부 숨긴다.
	const UTRTurretControlComponent* TurretControlComp = OwnerCharacter->GetTurretControlComponent();
	if (IsValid(TurretControlComp) && TurretControlComp->IsMounted())
	{
		HUDWidget->SetInteractionPrompts(FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty());
		return;
	}

	const AActor* Target = LastTraceResult.Get();
	if (!IsValid(Target))
	{
		HUDWidget->SetInteractionPrompts(FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty());
		return;
	}

	const ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target);
	const ATRTrain* Train = Cast<ATRTrain>(Target);
	const bool bNeedsRepair =
		(IsValid(Part) && !Part->IsDestroyed() && !Part->IsFullHP()) ||
		(IsValid(Train) && !Train->IsDestroyed() && !Train->IsFullHP());

	FText RepairPrompt;
	FText ReloadPrompt;
	FText InteractPrompt;

	if (bNeedsRepair)
	{
		RepairPrompt = FText::FromString(TEXT("클릭해서 수리"));
	}

	if (const ATRTurret* Turret = Cast<ATRTurret>(Target))
	{
		if (Turret->GetCurrentAmmo() < Turret->GetMaxAmmo())
		{
			ReloadPrompt = FText::FromString(TEXT("R 키를 눌러 재장전"));
		}

		if (!Turret->IsDestroyed() && !Turret->IsOccupied())
		{
			InteractPrompt = FText::FromString(TEXT("E 키를 눌러 포탑 탑승"));
		}
	}
	else if (Target->IsA<ATRSupplyPoint>())
	{
		InteractPrompt = FText::FromString(TEXT("E 키를 눌러 자원 획득"));
	}

	HUDWidget->SetInteractionPrompts(RepairPrompt, ReloadPrompt, InteractPrompt);
}


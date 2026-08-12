// TRInteractionComponent.cpp


#include "Interaction/TRInteractionComponent.h"
#include "Player/TRPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "UI/TRPlayerHUD.h"
#include "UI/TRPlayerHUDWidget.h"
#include "Interaction/TRTurretControlComponent.h"
#include "Interaction/TRActionComponent.h"
#include "Core/TRPlayerState.h"
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

	if (!Cast<ATRPlayerCharacter>(GetOwner()))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Owner가 ATRPlayerCharacter가 아니라 상호작용 트레이스와 HUD가 동작하지 않습니다."), *GetName());
	}
}


void UTRInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* OwnerPawn = GetOwner<APawn>();
	const bool bNeedsTrace = IsValid(OwnerPawn) && (OwnerPawn->HasAuthority() || OwnerPawn->IsLocallyControlled());
	if (!bNeedsTrace)
	{
		return;
	}

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

static void BuildPrompts(const AActor* Target, bool bHasSupplySpace, FText& OutRepair, FText& OutReload, FText& OutInteract)
{
	if (!IsValid(Target))
	{
		return;
	}

	// 파괴된 파츠는 수리가 아니라 제작(Build) 대상이므로 제외
	const ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target);
	const ATRTrain* Train = Cast<ATRTrain>(Target);
	if ((Part && Part->CanBeRepaired()) || (Train && Train->CanBeRepaired()))
	{
		OutRepair = FText::FromString(TEXT("클릭해서 수리"));
	}

	if (const ATRTurret* Turret = Cast<ATRTurret>(Target))
	{
		if (Turret->CanBeReloaded())
		{
			OutReload = FText::FromString(TEXT("R 키를 눌러 재장전"));
		}

		if (Turret->CanBeMounted())
		{
			OutInteract = FText::FromString(TEXT("E 키를 눌러 포탑 탑승"));
		}
	}
	else if (Target->IsA<ATRSupplyPoint>() && bHasSupplySpace)
	{
		OutInteract = FText::FromString(TEXT("E 키를 눌러 자원 획득"));
	}
}

UTRPlayerHUDWidget* UTRInteractionComponent::GetHUDWidget() const
{
	const APawn* OwnerPawn = GetOwner<APawn>();
	if (!IsValid(OwnerPawn))
	{
		return nullptr;
	}

	const APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!IsValid(PC))
	{
		return nullptr;
	}

	const ATRPlayerHUD* PlayerHUD = Cast<ATRPlayerHUD>(PC->GetHUD());
	if (!IsValid(PlayerHUD))
	{
		return nullptr;
	}

	return PlayerHUD->GetPlayerHUDWidget();
}

void UTRInteractionComponent::UpdateInteractionWidget()
{
	const ATRPlayerCharacter* OwnerCharacter = Cast<ATRPlayerCharacter>(GetOwner());
	UTRPlayerHUDWidget* HUDWidget = GetHUDWidget();
	if (!IsValid(OwnerCharacter) || !IsValid(HUDWidget))
	{
		return;
	}

	const UTRActionComponent* ActionComp = OwnerCharacter->GetActionComponent();
	const ETRActionType CurrentAction = IsValid(ActionComp) ? ActionComp->GetCurrentActionType() : ETRActionType::None;
	HUDWidget->SetActionProgress(CurrentAction, IsValid(ActionComp) ? ActionComp->GetActionProgress01() : 0.f);

	const ATRPlayerState* PlayerState = OwnerCharacter->GetPlayerState<ATRPlayerState>();
	if (IsValid(PlayerState))
	{
		HUDWidget->SetCarriedSupply(PlayerState->GetCarriedSupply());
	}

	HUDWidget->SetTargetInfo(LastTraceResult.Get());

	// 터렛에 탑승 중이면 수리도 재장전도 못 함
	const UTRTurretControlComponent* TurretControlComp = OwnerCharacter->GetTurretControlComponent();
	const bool bMounted = IsValid(TurretControlComp) && TurretControlComp->IsMounted();

	FText RepairPrompt;
	FText ReloadPrompt;
	FText InteractPrompt;
	if (!bMounted)
	{
		const bool bHasSupplySpace = IsValid(PlayerState) && PlayerState->HasSupplySpace();
		BuildPrompts(LastTraceResult.Get(), bHasSupplySpace, RepairPrompt, ReloadPrompt, InteractPrompt);
	}

	HUDWidget->SetInteractionPrompts(RepairPrompt, ReloadPrompt, InteractPrompt);
}

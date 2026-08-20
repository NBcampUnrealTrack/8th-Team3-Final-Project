// TRInteractionComponent.cpp


#include "Interaction/TRInteractionComponent.h"
#include "Debug/TRGameplayLog.h"
#include "TRTypes.h"
#include "Core/TRGameState.h"
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
#include "Train/TRControlConsole.h"
#include "Train/TRRepairTargetComponent.h"
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
		UE_LOG(LogTRGameplay, Error, TEXT("%s: Owner가 ATRPlayerCharacter가 아니라 상호작용 트레이스와 HUD가 동작하지 않습니다."), *GetName());
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

	// 방향은 카메라가 아니라 폰의 시점 회전에서 가져온다.
	// bUsePawnControlRotation 은 로컬 플레이어 컨트롤러일 때만 카메라를 돌리므로,
	// 서버에서 원격 클라의 카메라는 조준 방향이 아니라 머리 본 방향을 가리킨다.
	// ControlRotation 은 서버로 복제되니 이쪽을 써야 서버 검증이 클라와 같은 대상을 본다.
	const FVector Start = CameraComp->GetComponentLocation();
	const FVector End = Start + OwnerCharacter->GetViewRotation().Vector() * TraceDistance;

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	// 판 시작 전에는 전용 채널로 쏜다. 포탑·장갑·기차는 그 채널을 Ignore 하므로 아예 잡히지 않고,
	// GetCurrentTarget() 을 재확인하는 상호작용 경로들이 서버·UI 양쪽에서 함께 닫힌다.
	const ATRGameState* GameState = World->GetGameState<ATRGameState>();
	const bool bMatchStarted = IsValid(GameState) && GameState->IsMatchStarted();

	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		bMatchStarted ? ECC_Visibility : TRCollisionChannel::LobbyInteract,
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

		// 남이 타고 있으면 서버가 탑승을 거부하므로, 조용히 비워두지 않고 이유를 보여 준다.
		if (Turret->CanBeMounted())
		{
			OutInteract = FText::FromString(TEXT("E 키를 눌러 포탑 탑승"));
		}
		else if (Turret->IsOccupied())
		{
			OutInteract = FText::FromString(TEXT("포탑 탑승 중"));
		}
	}
	else if (Target->IsA<ATRSupplyPoint>() && bHasSupplySpace)
	{
		OutInteract = FText::FromString(TEXT("E 키를 눌러 자원 획득"));
	}
	else if (const ATRControlConsole* Console = Cast<ATRControlConsole>(Target))
	{
		OutInteract = Console->IsOccupied()
			? FText::FromString(TEXT("관제석 사용 중"))
			: FText::FromString(TEXT("E 키를 눌러 관제석 조작"));
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

float UTRInteractionComponent::ResolveActionProgress01(const UTRActionComponent& ActionComp, ETRActionType ActionType) const
{
	// 수리는 사이클을 대상이 갖는다 — 같은 대상을 수리하는 전원이 같은 게이지를 봐야 하므로,
	// 사람마다 시작 시각이 다른 내 타이머가 아니라 대상의 사이클을 읽는다.
	if (ActionType != ETRActionType::Repair)
	{
		return ActionComp.GetActionProgress01();
	}

	const AActor* Target = ActionComp.GetCurrentActionTarget();
	const UTRRepairTargetComponent* RepairTarget =
		IsValid(Target) ? Target->FindComponentByClass<UTRRepairTargetComponent>() : nullptr;

	return IsValid(RepairTarget) ? RepairTarget->GetCycleProgress01() : 0.f;
}

void UTRInteractionComponent::UpdateInteractionWidget()
{
	const ATRPlayerCharacter* OwnerCharacter = Cast<ATRPlayerCharacter>(GetOwner());
	UTRPlayerHUDWidget* HUDWidget = GetHUDWidget();
	if (!IsValid(OwnerCharacter) || !IsValid(HUDWidget))
	{
		return;
	}

	if (OwnerCharacter->IsOccupyingControlConsole())
	{
		HUDWidget->SetActionProgress(ETRActionType::None, 0.f);
		HUDWidget->SetTargetInfo(nullptr);
		HUDWidget->SetInteractionPrompts(FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty());
		return;
	}

	const UTRActionComponent* ActionComp = OwnerCharacter->GetActionComponent();
	const ETRActionType CurrentAction = IsValid(ActionComp) ? ActionComp->GetCurrentActionType() : ETRActionType::None;
	const float ActionProgress = IsValid(ActionComp) ? ResolveActionProgress01(*ActionComp, CurrentAction) : 0.f;
	HUDWidget->SetActionProgress(CurrentAction, ActionProgress);

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

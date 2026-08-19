// TRNameplateComponent.cpp

#include "Player/TRNameplateComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UI/TRNameplateWidget.h"
#include "UObject/ConstructorHelpers.h"

UTRNameplateComponent::UTRNameplateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 원근이 먹도록 월드 공간에 그린다. 화면 공간이면 멀어져도 크기가 그대로다.
	SetWidgetSpace(EWidgetSpace::World);
	SetDrawAtDesiredSize(true);
	SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	SetRelativeScale3D(FVector(0.1f));
	SetCullDistance(1500.f);

	// 기본 표시 위젯. 블루프린트에서 WidgetClass 를 덮어쓸 수 있다.
	static ConstructorHelpers::FClassFinder<UTRNameplateWidget> DefaultNameplate(TEXT("/Game/UI/WBP_TRNameplate"));
	if (DefaultNameplate.Succeeded())
	{
		SetWidgetClass(DefaultNameplate.Class);
	}

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}

void UTRNameplateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	if (bHideForLocalOwner && OwnerPawn->IsLocallyControlled())
	{
		SetVisibility(false);
		return;
	}

	SetVisibility(true);

	FaceLocalCamera();
	RefreshName();
}

void UTRNameplateComponent::FaceLocalCamera()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
	SetWorldRotation(FRotator(0.f, CameraRotation.Yaw + 180.f, 0.f));
}

void UTRNameplateComponent::RefreshName()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const APlayerState* PS = OwnerPawn ? OwnerPawn->GetPlayerState() : nullptr;
	if (!PS)
	{
		return;
	}

	const FString PlayerName = PS->GetPlayerName();
	if (PlayerName == CachedName)
	{
		return;
	}

	UTRNameplateWidget* Nameplate = Cast<UTRNameplateWidget>(GetUserWidgetObject());
	if (!Nameplate)
	{
		return;
	}

	CachedName = PlayerName;
	Nameplate->SetPlayerName(FText::FromString(PlayerName));
}

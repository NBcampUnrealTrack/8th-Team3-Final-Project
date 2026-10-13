#include "UI/Target/BGCameraLookComponent.h"

#include "Blueprint/UserWidget.h"
#include "Core/BGCharacter.h"
#include "Core/BGPlayerController.h"
#include "UI/Target/BGWorldTargetingComponent.h"

UBGCameraLookComponent::UBGCameraLookComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool UBGCameraLookComponent::IsMouseOverGameplayHud(const ABGPlayerController& PC)
{
	TArray<UWidget*> HudWidgets;
	PC.CollectGameplayHudWidgets(HudWidgets);

	for (const UWidget* Widget : HudWidgets)
	{
		if (IsValid(Widget) && Widget->IsHovered())
		{
			return true;
		}
	}
	return false;
}

float UBGCameraLookComponent::ComputeTargetYaw(const ABGPlayerController& PC, float MaxYaw)
{
	float MouseX = 0.f, MouseY = 0.f;
	if (!PC.GetMousePosition(MouseX, MouseY))
	{
		return 0.f;
	}

	int32 ViewportX = 0, ViewportY = 0;
	PC.GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0)
	{
		return 0.f;
	}

	//	0(왼쪽 끝) ~ 1(오른쪽 끝). 0.5가 정면이다.
	const float Ratio = FMath::Clamp(MouseX / static_cast<float>(ViewportX), 0.f, 1.f);
	return (Ratio - 0.5f) * 2.f * MaxYaw;
}

void UBGCameraLookComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwner());
	if (!PC || !PC->HasLocalViewer())
	{
		return;
	}

	//	조준 중엔 BGWorldTargetingComponent 가 같은 카메라의 Yaw 오프셋을 쓴다 — 쉰다.
	if (const UBGWorldTargetingComponent* Targeting = PC->FindComponentByClass<UBGWorldTargetingComponent>())
	{
		if (Targeting->IsTargeting())
		{
			return;
		}
	}

	ABGCharacter* Character = Cast<ABGCharacter>(PC->GetPawn());
	if (!Character)
	{
		return;   // 좌석에 아직 앉기 전(로비 등) — 돌릴 카메라가 없다.
	}

	//	마우스가 UI 위면 "게임 플레이 화면" 이 아니다 — 가운데로 돌려보낸다.
	const bool bInGameplayArea = PC->bShowMouseCursor && !IsMouseOverGameplayHud(*PC);
	const float TargetYaw = bInGameplayArea ? ComputeTargetYaw(*PC, MaxYawDeg) : 0.f;

	CurrentYawDeg = FMath::FInterpConstantTo(CurrentYawDeg, TargetYaw, DeltaTime, FollowSpeedDeg);
	Character->SetPrimaryCameraYawOffset(CurrentYawDeg);
}

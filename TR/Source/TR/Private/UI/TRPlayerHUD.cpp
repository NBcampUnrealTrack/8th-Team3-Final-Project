// TRPlayerHUD.cpp

#include "UI/TRPlayerHUD.h"
#include "Blueprint/UserWidget.h"
#include "Debug/TRGameplayLog.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SViewport.h"
#include "UI/TRPlayerHUDWidget.h"
#include "UI/TRControlConsoleWidget.h"

static UGameViewportClient* GetViewportClient(const APlayerController* PC)
{
	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
}

void ATRPlayerHUD::ApplyGameInputMode(APlayerController* PC)
{
	if (!IsValid(PC))
	{
		return;
	}

	FInputModeGameOnly Mode;
	Mode.SetConsumeCaptureMouseDown(true);

	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(false);

	const UGameViewportClient* ViewportClient = GetViewportClient(PC);
	const TSharedPtr<SViewport> ViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr;
	if (ViewportWidget.IsValid())
	{
		PC->GetLocalPlayer()->GetSlateOperations().CaptureMouse(ViewportWidget.ToSharedRef());
	}

	if (ATRPlayerHUD* HUD = PC->GetHUD<ATRPlayerHUD>())
	{
		HUD->bCenterMouse = true;
	}
}

void ATRPlayerHUD::ApplyPopupInputMode(APlayerController* PC, UUserWidget* FocusWidget)
{
	if (!IsValid(PC))
	{
		return;
	}

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (IsValid(FocusWidget))
	{
		Mode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}

	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);

	if (ATRPlayerHUD* HUD = PC->GetHUD<ATRPlayerHUD>())
	{
		HUD->bCenterMouse = false;
	}
}

void ATRPlayerHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = GetOwningPlayerController();
	if (!bCenterMouse || !IsValid(PC))
	{
		return;
	}

	// 뷰포트를 다시 클릭하면 캡처가 돌아오고 중앙 고정
	const UGameViewportClient* ViewportClient = GetViewportClient(PC);
	const FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (Viewport == nullptr || !Viewport->HasMouseCapture())
	{
		return;
	}

	int32 SizeX = 0;
	int32 SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	if (SizeX <= 0 || SizeY <= 0)
	{
		return;
	}

	PC->SetMouseLocation(SizeX / 2, SizeY / 2);
}

void ATRPlayerHUD::BeginPlay()
{
	Super::BeginPlay();

	ApplyGameInputMode(GetOwningPlayerController());

	if (!IsValid(PlayerHUDWidgetClass))
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("PlayerHUDWidgetClass is not set"));
		return;
	}

	PlayerHUDWidget = CreateWidget<UTRPlayerHUDWidget>(GetOwningPlayerController(), PlayerHUDWidgetClass);
	if (IsValid(PlayerHUDWidget))
	{
		PlayerHUDWidget->AddToViewport();
	}
}

static constexpr int32 ControlConsoleZOrder = 100;

void ATRPlayerHUD::ToggleControlConsole()
{
	if (IsControlConsoleOpen())
	{
		CloseControlConsole();
		return;
	}

	if (!IsValid(ControlConsoleWidgetClass))
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("ControlConsoleWidgetClass is not set"));
		return;
	}

	ControlConsoleWidget = CreateWidget<UTRControlConsoleWidget>(GetOwningPlayerController(), ControlConsoleWidgetClass);
	if (IsValid(ControlConsoleWidget))
	{
		ControlConsoleWidget->AddToViewport(ControlConsoleZOrder);
	}
}

void ATRPlayerHUD::CloseControlConsole()
{
	if (!IsControlConsoleOpen())
	{
		return;
	}

	// 커서와 입력 원복은 위젯의 NativeDestruct가 한다.
	ControlConsoleWidget->RemoveFromParent();
	ControlConsoleWidget = nullptr;
}

// TRPlayerHUD.cpp

#include "UI/TRPlayerHUD.h"
#include "Debug/TRGameplayLog.h"
#include "UI/TRPlayerHUDWidget.h"
#include "UI/TRControlConsoleWidget.h"

void ATRPlayerHUD::BeginPlay()
{
	Super::BeginPlay();

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

// TRPlayerHUD.cpp

#include "UI/TRPlayerHUD.h"
#include "Debug/TRGameplayLog.h"
#include "UI/TRPlayerHUDWidget.h"

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

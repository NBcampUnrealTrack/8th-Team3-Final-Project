// TRPlayerHUD.cpp

#include "UI/TRPlayerHUD.h"
#include "UI/TRPlayerHUDWidget.h"

void ATRPlayerHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerHUDWidgetClass)
	{
		return;
	}

	PlayerHUDWidget = CreateWidget<UTRPlayerHUDWidget>(GetOwningPlayerController(), PlayerHUDWidgetClass);
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->AddToViewport();
	}
}

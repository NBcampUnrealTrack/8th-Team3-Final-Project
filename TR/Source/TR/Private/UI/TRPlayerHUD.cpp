// TRPlayerHUD.cpp


#include "UI/TRPlayerHUD.h"
#include "Blueprint/UserWidget.h"

void ATRPlayerHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerHUDWidgetClass)
	{
		return;
	}

	PlayerHUDWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), PlayerHUDWidgetClass);
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->AddToViewport();
	}
}

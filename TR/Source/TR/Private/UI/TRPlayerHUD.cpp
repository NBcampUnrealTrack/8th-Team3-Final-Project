// TRPlayerHUD.cpp

#include "UI/TRPlayerHUD.h"
#include "UI/TRPlayerHUDWidget.h"

// 설정된 HUD 위젯 클래스를 생성해 플레이어 화면에 표시
void ATRPlayerHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(PlayerHUDWidgetClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidgetClass is not set"));
		return;
	}

	PlayerHUDWidget = CreateWidget<UTRPlayerHUDWidget>(GetOwningPlayerController(), PlayerHUDWidgetClass);
	if (IsValid(PlayerHUDWidget))
	{
		PlayerHUDWidget->AddToViewport();
	}
}

// TRPlayerHUDWidget.cpp

#include "UI/TRPlayerHUDWidget.h"
#include "Components/Widget.h"

void UTRPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetInteractionVisible(false);
}

void UTRPlayerHUDWidget::SetInteractionVisible(bool bVisible)
{
	if (!IsValid(PlayerInteraction))
	{
		return;
	}

	PlayerInteraction->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

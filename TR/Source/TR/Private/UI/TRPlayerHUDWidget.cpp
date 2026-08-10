// TRPlayerHUDWidget.cpp

#include "UI/TRPlayerHUDWidget.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"

void UTRPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetInteractionPrompts(FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty());
	SetRepairProgressVisible(false);
}

// 한 줄을 문구에 맞춰 켜고 끈다. 빈 문구면 그 줄만 숨김.
static void ApplyPromptLine(UWidget* Line, UTextBlock* Text, const FText& Prompt)
{
	if (!IsValid(Line))
	{
		return;
	}

	const bool bVisible = !Prompt.IsEmpty();
	Line->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (bVisible && IsValid(Text))
	{
		Text->SetText(Prompt);
	}
}

void UTRPlayerHUDWidget::SetInteractionPrompts(const FText& RepairPrompt, const FText& ReloadPrompt, const FText& InteractPrompt)
{
	if (!IsValid(PlayerInteraction_VerticalBox))
	{
		return;
	}

	ApplyPromptLine(PlayerInteraction_Repair, Txt_Repair, RepairPrompt);
	ApplyPromptLine(PlayerInteraction_Reload, Txt_Reload, ReloadPrompt);
	ApplyPromptLine(PlayerInteraction_Interact, Txt_Interact, InteractPrompt);

	const bool bAnyVisible = !RepairPrompt.IsEmpty() || !ReloadPrompt.IsEmpty() || !InteractPrompt.IsEmpty();
	PlayerInteraction_VerticalBox->SetVisibility(bAnyVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UTRPlayerHUDWidget::SetRepairProgressVisible(bool bVisible)
{
	if (!IsValid(RepairProgressPanel))
	{
		return;
	}

	RepairProgressPanel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

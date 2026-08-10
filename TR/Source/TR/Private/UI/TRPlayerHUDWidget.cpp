// TRPlayerHUDWidget.cpp

#include "UI/TRPlayerHUDWidget.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/TRPlayerState.h"

void UTRPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ensureMsgf(IsValid(SupplySlotBox) && SupplySlotBox->GetChildrenCount() == ATRPlayerState::MaxCarriedSupply,
		TEXT("자원 슬롯 개수(%d)와 MaxCarriedSupply(%d)가 다릅니다."),
		IsValid(SupplySlotBox) ? SupplySlotBox->GetChildrenCount() : 0,
		ATRPlayerState::MaxCarriedSupply);

	SetInteractionPrompts(FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty());
	SetRepairProgressVisible(false);
	SetCarriedSupply(0);
}

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

void UTRPlayerHUDWidget::SetRepairProgress(float Progress01)
{
	if (IsValid(RepairPercent))
	{
		RepairPercent->SetText(FText::AsPercent(Progress01));
	}

	if (IsValid(RepairProgressValue))
	{
		if (UMaterialInstanceDynamic* FillMaterial = RepairProgressValue->GetDynamicMaterial())
		{
			static const FName PercentParam(TEXT("Percent"));
			FillMaterial->SetScalarParameterValue(PercentParam, Progress01);
		}
	}
}

void UTRPlayerHUDWidget::SetCarriedSupply(int32 Count)
{
	if (!IsValid(SupplySlotBox))
	{
		return;
	}

	for (int32 Index = 0; Index < SupplySlotBox->GetChildrenCount(); ++Index)
	{
		if (UWidget* SupplyIcon = SupplySlotBox->GetChildAt(Index))
		{
			SupplyIcon->SetVisibility(Index < Count ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
	}
}

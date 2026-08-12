// TRPlayerHUDWidget.cpp

#include "UI/TRPlayerHUDWidget.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/TRPlayerState.h"

// WBP 바인딩 상태 검증 및 HUD 초기 상태 설정
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

// 안내 문구 유무에 맞춰 개별 상호작용 행 표시
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

// 수리/재장전/상호작용 안내와 상위 패널 표시 상태 갱신
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

// 수리 진행 패널 표시 여부 갱신
void UTRPlayerHUDWidget::SetRepairProgressVisible(bool bVisible)
{
	if (!IsValid(RepairProgressPanel))
	{
		return;
	}

	RepairProgressPanel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

// 수리 진행률을 텍스트와 Material 파라미터에 함께 반영
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

// 보유 개수 이하 슬롯만 표시해 현재 자원 수 시각화
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

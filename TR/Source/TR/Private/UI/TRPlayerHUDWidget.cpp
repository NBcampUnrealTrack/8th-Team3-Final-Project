// TRPlayerHUDWidget.cpp

#include "UI/TRPlayerHUDWidget.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/TRPlayerState.h"
#include "Core/TRGameState.h"
#include "Train/TRArmor.h"
#include "Train/TRTrain.h"
#include "Train/TRTurret.h"

void UTRPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ensureMsgf(IsValid(SupplySlotBox) && SupplySlotBox->GetChildrenCount() == ATRPlayerState::MaxCarriedSupply,
		TEXT("자원 슬롯 개수(%d)와 MaxCarriedSupply(%d)가 다릅니다."),
		IsValid(SupplySlotBox) ? SupplySlotBox->GetChildrenCount() : 0,
		ATRPlayerState::MaxCarriedSupply);

	SetInteractionPrompts(FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty());
	SetActionProgress(ETRActionType::None, 0.f);
	SetCarriedSupply(0);
	SetTargetInfo(nullptr);
	SetCrosshairVisible(false);
	SetTurretControlsVisible(false);
}

void UTRPlayerHUDWidget::SetCrosshairVisible(bool bVisible)
{
	if (!IsValid(CrossHair))
	{
		return;
	}

	CrossHair->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UTRPlayerHUDWidget::SetTurretControlsVisible(bool bMounted)
{
	const ESlateVisibility OnFootVisibility = bMounted ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	const ESlateVisibility MountedVisibility = bMounted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	for (UWidget* OnFootLine : { Repair.Get(), Cancel.Get(), Move.Get() })
	{
		if (IsValid(OnFootLine))
		{
			OnFootLine->SetVisibility(OnFootVisibility);
		}
	}

	if (IsValid(Fire))
	{
		Fire->SetVisibility(MountedVisibility);
	}
}

// 빈 텍스트면 줄을 접고, 아니면 표시 및 텍스트 갱신
static void ApplyTextLine(UWidget* Line, UTextBlock* Text, const FText& Value)
{
	if (!IsValid(Line))
	{
		return;
	}

	const bool bVisible = !Value.IsEmpty();
	Line->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (bVisible && IsValid(Text))
	{
		Text->SetText(Value);
	}
}

void UTRPlayerHUDWidget::SetInteractionPrompts(const FText& RepairPrompt, const FText& ReloadPrompt, const FText& InteractPrompt)
{
	if (!IsValid(PlayerInteraction_VerticalBox))
	{
		return;
	}

	ApplyTextLine(PlayerInteraction_Repair, Txt_Interact1, RepairPrompt);
	ApplyTextLine(PlayerInteraction_Reload, Txt_Interact2, ReloadPrompt);
	ApplyTextLine(PlayerInteraction_Interact, Txt_Interact3, InteractPrompt);

	const bool bAnyVisible = !RepairPrompt.IsEmpty() || !ReloadPrompt.IsEmpty() || !InteractPrompt.IsEmpty();
	PlayerInteraction_VerticalBox->SetVisibility(bAnyVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UTRPlayerHUDWidget::SetActionProgress(ETRActionType ActionType, float Progress01)
{
	FText ActionText;
	switch (ActionType)
	{
		case ETRActionType::Repair:
			ActionText = FText::FromString(TEXT("수리 중"));
			break;
		case ETRActionType::Reload:
			ActionText = FText::FromString(TEXT("장전 중"));
			break;
		case ETRActionType::Restock:
			ActionText = FText::FromString(TEXT("보급 중"));
			break;
		default:
			break;
	}

	ApplyTextLine(ProgressPanel, InteractText, ActionText);
	if (ActionText.IsEmpty())
	{
		return;
	}

	if (IsValid(InteractPercent))
	{
		InteractPercent->SetText(FText::AsPercent(Progress01));
	}

	if (IsValid(ProgressValue))
	{
		if (UMaterialInstanceDynamic* FillMaterial = ProgressValue->GetDynamicMaterial())
		{
			static const FName PercentParam(TEXT("Percent"));
			FillMaterial->SetScalarParameterValue(PercentParam, Progress01);
		}
	}
}

static FText MakeLevelText(int32 Level)
{
	return Level > 0 ? FText::FromString(FString::Printf(TEXT("LV. %d"), Level)) : FText::GetEmpty();
}

static FText MakeHPText(float CurrentHP, float MaxHP)
{
	return FText::FromString(FString::Printf(TEXT("HP  %d/%d"), FMath::CeilToInt(CurrentHP), FMath::CeilToInt(MaxHP)));
}

void UTRPlayerHUDWidget::SetTargetInfo(const AActor* Target)
{
	if (!IsValid(PlayerInfoPanel))
	{
		return;
	}

	// 레벨은 각 파츠별 전체 공유
	const ATRGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ATRGameState>() : nullptr;

	FText LevelText;
	FText HPText;
	FText AmmoText;

	if (const ATRTurret* Turret = Cast<ATRTurret>(Target))
	{
		LevelText = MakeLevelText(IsValid(GameState) ? GameState->GetTurretLevel() : 0);
		HPText = MakeHPText(Turret->GetCurrentHP(), Turret->GetMaxHP());
		AmmoText = FText::FromString(FString::Printf(TEXT("AMMO  %d/%d"), Turret->GetCurrentAmmo(), Turret->GetMaxAmmo()));
	}
	else if (const ATRArmor* Armor = Cast<ATRArmor>(Target))
	{
		LevelText = MakeLevelText(IsValid(GameState) ? GameState->GetArmorLevel() : 0);
		HPText = MakeHPText(Armor->GetCurrentHP(), Armor->GetMaxHP());
	}
	else if (const ATRTrain* Train = Cast<ATRTrain>(Target))
	{
		HPText = MakeHPText(Train->GetCurrentHullHealth(), Train->GetMaxHullHealth());
	}

	// HP를 기준으로 비어 있으면 정보 없는 대상
	PlayerInfoPanel->SetVisibility(HPText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	ApplyTextLine(LevelPanel, Txt_Level, LevelText);
	ApplyTextLine(HPPanel, Txt_HP, HPText);
	ApplyTextLine(AMMOBorder, Txt_AMMO, AmmoText);
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

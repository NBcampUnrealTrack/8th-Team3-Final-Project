// TRPlayerHUDWidget.h

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TRPlayerHUDWidget.generated.h"

class UWidget;
class UTextBlock;
class UImage;
class UPanelWidget;

UCLASS(Abstract)
class TR_API UTRPlayerHUDWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetInteractionPrompts(const FText& RepairPrompt, const FText& ReloadPrompt, const FText& InteractPrompt);

	void SetRepairProgressVisible(bool bVisible);

	void SetRepairProgress(float Progress01);

	void SetCarriedSupply(int32 Count);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_VerticalBox;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> SupplySlotBox;

	// 수리 (좌클릭)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_Repair;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Repair;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RepairPercent;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> RepairProgressValue;

	// 재장전 (R)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_Reload;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Reload;

	// 탑승 / 자원 획득 (E)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_Interact;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Interact;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> RepairProgressPanel;
};

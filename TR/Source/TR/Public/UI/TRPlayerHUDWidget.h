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
	// 현재 상호작용 대상에 맞춰 수리/재장전/상호작용 안내 갱신
	void SetInteractionPrompts(const FText& RepairPrompt, const FText& ReloadPrompt, const FText& InteractPrompt);

	// 수리 진행 UI 표시 여부와 진행률 갱신
	void SetRepairProgressVisible(bool bVisible);
	void SetRepairProgress(float Progress01);

	// 보유 자원 수에 맞춰 슬롯 아이콘 갱신
	void SetCarriedSupply(int32 Count);

protected:
	virtual void NativeConstruct() override;

	//--- 상호작용 안내 ---
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_VerticalBox;

	//--- 보유 자원 ---
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

	//--- 수리 진행률 ---
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> RepairProgressPanel;
};

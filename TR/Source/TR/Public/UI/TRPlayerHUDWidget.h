// TRPlayerHUDWidget.h

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TRTypes.h"
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

	// 진행 중인 액션의 이름과 진행률 표시
	void SetActionProgress(ETRActionType ActionType, float Progress01);

	void SetCarriedSupply(int32 Count);

	// 상호작용 대상의 레벨/HP/탄약 표시
	void SetTargetInfo(const AActor* Target);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_VerticalBox;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> SupplySlotBox;

	// 화면 우측 위 정보 패널
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInfoPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> LevelPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Level;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> HPPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_HP;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> AMMOBorder;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_AMMO;

	// 화면 중앙 진행도(원형) 패널
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ProgressPanel;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> InteractText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> InteractPercent;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ProgressValue;
	
	// 수리 (좌클릭)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_Repair;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Interact1;

	// 재장전 (R)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_Reload;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Interact2;

	// 탑승 / 자원 획득 (E)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_Interact;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Interact3;
};

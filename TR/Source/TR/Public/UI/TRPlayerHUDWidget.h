// TRPlayerHUDWidget.h

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TRPlayerHUDWidget.generated.h"

class UWidget;
class UTextBlock;

UCLASS(Abstract)
class TR_API UTRPlayerHUDWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 각 줄은 빈 문구를 넘기면 숨는다. 셋 다 비면 패널 전체가 숨는다.
	void SetInteractionPrompts(const FText& RepairPrompt, const FText& ReloadPrompt, const FText& InteractPrompt);

	void SetRepairProgressVisible(bool bVisible);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_VerticalBox;

	// 수리 (좌클릭)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction_Repair;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Repair;

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

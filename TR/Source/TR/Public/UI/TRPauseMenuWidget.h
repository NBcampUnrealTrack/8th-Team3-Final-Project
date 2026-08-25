// TRPauseMenuWidget.h

#pragma once

#include "CommonActivatableWidget.h"
#include "TRPauseMenuWidget.generated.h"

class UTRSettingsCommonButton;
class UTRSettingsWidget;

// 게임 중 ESC 로 여는 퍼즈 메뉴. 세션 게임이라 시간은 멈추지 않고 오버레이만 띄운다.
UCLASS(Abstract)
class TR_API UTRPauseMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UTRPauseMenuWidget();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> Btn_Resume;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> Btn_Settings;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> Btn_Quit;

	UPROPERTY(EditDefaultsOnly, Category = "TR|UI")
	TSoftClassPtr<UTRSettingsWidget> SettingsWidgetClass;

private:
	void HandleResumeClicked();
	void HandleSettingsClicked();
	void HandleQuitClicked();

	void CloseSettings();

	// 여닫는 판단은 HUD 몫이라 그쪽에 넘긴다.
	void RequestClose();

	UPROPERTY(Transient)
	TObjectPtr<UTRSettingsWidget> SettingsPopup;

	ESlateVisibility VisibilityBeforeSettings = ESlateVisibility::SelfHitTestInvisible;

	bool bInputSuppressed = false;
};

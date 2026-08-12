// TRSettingsWidget.h

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Settings/TRSettingsTypes.h"
#include "TRSettingsWidget.generated.h"

class UTextBlock;
class UTRSettingComboWidget;
class UTRSettingInfoWidget;
class UTRSettingsCommonButton;
class UTRSettingsSubsystem;
class UWidgetSwitcher;

// 설정 데이터와 Designer에서 구성한 WBP_Settings를 연결하는 CommonUI 화면
UCLASS(Abstract, Blueprintable)
class TR_API UTRSettingsWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UTRSettingsWidget();
	virtual void NativeConstruct() override;

protected:
	//--- CommonUI 활성화 및 입력 처리 ---
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	//--- 공통 내비게이션 및 명령 버튼 ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> BackButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> GraphicsTabButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> AudioTabButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> ControlTabButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> SettingsSwitcher;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> DefaultsButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> CancelButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> ApplyButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsCommonButton> AutoDetectButton;

	//--- 그래픽 설정 행 ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> DisplayModeSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> DisplayMonitorSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> ResolutionSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> HDRSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> HDRUISeparateSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> HDRCalibrationSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingInfoWidget> HDRDisplayNitsSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingInfoWidget> HDRPaperWhiteSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingInfoWidget> HDRUILuminanceSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> OverallQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingInfoWidget> ResolutionScaleSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> VSyncSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> DynamicResolutionSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> FrameRateSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> ViewDistanceQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> AntiAliasingQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> ShadowQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> GlobalIlluminationQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> ReflectionQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> PostProcessQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> TextureQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> EffectsQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> FoliageQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> ShadingQualitySetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingComboWidget> LandscapeQualitySetting;

	//--- 오디오 및 조작 설정 행 ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingInfoWidget> MasterVolumeSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingInfoWidget> MusicVolumeSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingInfoWidget> EffectsVolumeSetting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingInfoWidget> SensitivitySetting;

private:
	// 설정 행 초기화 및 UTRSettingsSubsystem 값 동기화
	void InitializeRows();
	void RefreshFromSettings();
	UTRSettingsSubsystem* GetSettingsSubsystem() const;
	void SetStatus(const FString& Message, bool bSuccess);

	// 탭 전환과 선택 상태 동기화
	void SetGraphicsTab();

	//--- 설정값 변경 이벤트 ---
	UFUNCTION()
	void HandleSliderValueChanged(ETRSettingId SettingId, float NewValue);
	UFUNCTION()
	void HandleOptionChanged(ETRSettingId SettingId, const FString& SelectedOption);

	//--- 버튼 입력 이벤트 ---
	UFUNCTION()
	void HandleGraphicsTabClicked();
	UFUNCTION()
	void HandleAudioTabClicked();
	UFUNCTION()
	void HandleControlTabClicked();
	UFUNCTION()
	void HandleApplyClicked();
	UFUNCTION()
	void HandleCancelClicked();
	UFUNCTION()
	void HandleDefaultsClicked();
	UFUNCTION()
	void HandleAutoDetectClicked();
	UFUNCTION()
	void HandleBackClicked();
};

// TRSettingsWidget.h

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Settings/TRSettingsTypes.h"
#include "TRSettingsWidget.generated.h"

class UCommonAnimatedSwitcher;
class UCommonButtonBase;
class UTextBlock;
class UTRSettingComboWidget;
class UTRSettingInfoWidget;
class UTRSettingsCommonButton;
class UTRSettingsSubsystem;
class UTRSettingsTabList;

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
	// 탭 버튼은 이 위젯이 런타임에 만든다. 배타 선택도 이쪽이 맡는다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTRSettingsTabList> TabList;

	// 전환 애니메이션을 위해 UWidgetSwitcher 대신 CommonUI 쪽 스위처를 쓴다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonAnimatedSwitcher> SettingsSwitcher;

	// 탭이 가리키는 페이지. 스위처의 자식이라 이름만 맞으면 붙는다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> GraphicsPage;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> AudioPage;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> ControlPage;

	// 탭 줄에 찍어낼 버튼 WBP. WBP_TRSettings 의 클래스 디폴트에서 고른다.
	// 글자를 넣으려면 UTRSettingsCommonButton 이어야 한다 — 타입으로 막아 둔다.
	UPROPERTY(EditDefaultsOnly, Category = "TR|Settings|CommonUI")
	TSubclassOf<UTRSettingsCommonButton> TabButtonClass;
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

	// 탭 줄을 스위처에 잇고 세 페이지를 등록한다. 이후 전환은 CommonUI 가 맡는다.
	void SetUpTabs();

	// 버튼은 런타임에 만들어지므로 등록 후에 부른다.
	void ApplyTabLabel(FName TabId, const FText& InLabel) const;

	//--- 설정값 변경 이벤트 ---
	UFUNCTION()
	void HandleSliderValueChanged(ETRSettingId SettingId, float NewValue);
	UFUNCTION()
	void HandleOptionChanged(ETRSettingId SettingId, const FString& SelectedOption);

	//--- 버튼 입력 이벤트 ---
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

// TRSettingInfoWidget.h

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Settings/TRSettingsTypes.h"
#include "TRSettingInfoWidget.generated.h"

class USlider;
class USpinBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTRSettingValueChanged, ETRSettingId, SettingId, float, NewValue);

// 수치형 설정을 SpinBox와 Slider로 함께 표시하는 Designer 기반 행 위젯
UCLASS(Blueprintable)
class TR_API UTRSettingInfoWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// 설정 ID, 표시 이름, 수치 범위와 현재값 초기화
	UFUNCTION(BlueprintCallable, Category = "TR|Settings")
	void InitializeSetting(ETRSettingId InSettingId, const FText& InDisplayName, float InMinValue, float InMaxValue, float InStepSize, float InValue);

	// 외부 설정값 반영
	UFUNCTION(BlueprintCallable, Category = "TR|Settings")
	void SetValue(float InValue);
	UFUNCTION(BlueprintPure, Category = "TR|Settings")
	float GetValue() const { return CurrentValue; }

	UPROPERTY(BlueprintAssignable, Category = "TR|Settings")
	FTRSettingValueChanged OnSettingValueChanged;

protected:
	//--- WBP 바인딩 위젯 ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> SettingNameText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> ValueSlider;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USpinBox> ValueSpinBox;

private:
	// Slider와 SpinBox 값을 동기화하고 사용자 입력만 외부로 전파
	void SynchronizeControls(float InValue, bool bBroadcast);
	UFUNCTION()
	void HandleSliderValueChanged(float NormalizedValue);
	UFUNCTION()
	void HandleSpinBoxValueChanged(float NewValue);

	//--- 설정값 캐시 ---
	ETRSettingId SettingId = ETRSettingId::None;
	float MinValue = 0.0f;
	float MaxValue = 1.0f;
	float StepSize = 0.01f;
	float CurrentValue = 0.0f;
	bool bSynchronizingControls = false;
};

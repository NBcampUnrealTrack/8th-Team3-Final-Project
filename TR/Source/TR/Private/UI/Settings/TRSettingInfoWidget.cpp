// TRSettingInfoWidget.cpp

#include "UI/Settings/TRSettingInfoWidget.h"

#include "Brushes/SlateNoResource.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"

// Slider와 SpinBox 변경 이벤트 바인딩 및 초기값 동기화
void UTRSettingInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();
	FSpinBoxStyle SpinBoxStyle = ValueSpinBox->GetWidgetStyle();
	SpinBoxStyle.SetActiveFillBrush(FSlateNoResource());
	SpinBoxStyle.SetHoveredFillBrush(FSlateNoResource());
	SpinBoxStyle.SetInactiveFillBrush(FSlateNoResource());
	ValueSpinBox->SetWidgetStyle(SpinBoxStyle);
	ValueSlider->OnValueChanged.AddUniqueDynamic(this, &UTRSettingInfoWidget::HandleSliderValueChanged);
	ValueSpinBox->OnValueChanged.AddUniqueDynamic(this, &UTRSettingInfoWidget::HandleSpinBoxValueChanged);
	SynchronizeControls(CurrentValue, false);
}

// 설정 메타데이터와 입력 가능 범위를 각 컨트롤에 적용
void UTRSettingInfoWidget::InitializeSetting(const ETRSettingId InSettingId, const FText& InDisplayName, const float InMinValue,
	const float InMaxValue, const float InStepSize, const float InValue)
{
	SettingId = InSettingId;
	MinValue = InMinValue;
	MaxValue = FMath::Max(InMaxValue, InMinValue + UE_SMALL_NUMBER);
	StepSize = FMath::Max(InStepSize, UE_SMALL_NUMBER);
	SettingNameText->SetText(InDisplayName);
	ValueSpinBox->SetMinValue(MinValue);
	ValueSpinBox->SetMaxValue(MaxValue);
	ValueSpinBox->SetMinSliderValue(MinValue);
	ValueSpinBox->SetMaxSliderValue(MaxValue);
	ValueSpinBox->SetDelta(StepSize);
	SynchronizeControls(InValue, false);
}

void UTRSettingInfoWidget::SetValue(const float InValue)
{
	SynchronizeControls(InValue, false);
}

// 값 범위 제한 후 Slider/SpinBox를 재귀 이벤트 없이 동기화
void UTRSettingInfoWidget::SynchronizeControls(const float InValue, const bool bBroadcast)
{
	const float ClampedValue = FMath::Clamp(InValue, MinValue, MaxValue);
	CurrentValue = FMath::Clamp(MinValue + FMath::GridSnap(ClampedValue - MinValue, StepSize), MinValue, MaxValue);
	TGuardValue<bool> Guard(bSynchronizingControls, true);
	ValueSlider->SetValue((CurrentValue - MinValue) / (MaxValue - MinValue));
	ValueSpinBox->SetValue(CurrentValue);
	if (bBroadcast)
	{
		OnSettingValueChanged.Broadcast(SettingId, CurrentValue);
	}
}

// 0~1 Slider 입력을 실제 설정 범위로 변환
void UTRSettingInfoWidget::HandleSliderValueChanged(const float NormalizedValue)
{
	if (!bSynchronizingControls)
	{
		SynchronizeControls(FMath::Lerp(MinValue, MaxValue, NormalizedValue), true);
	}
}

// SpinBox 입력을 설정 단위에 맞춰 보정
void UTRSettingInfoWidget::HandleSpinBoxValueChanged(const float NewValue)
{
	if (!bSynchronizingControls)
	{
		SynchronizeControls(NewValue, true);
	}
}

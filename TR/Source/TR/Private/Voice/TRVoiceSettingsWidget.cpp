// TRVoiceSettingsWidget.cpp

#include "Voice/TRVoiceSettingsWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Voice/TRVoiceSubsystem.h"

namespace TRVoiceUI
{
	// 슬라이더는 0~1 이라 실제 범위로 환산해서 쓴다.
	static constexpr float MaxMicGain = 2.f;
	static constexpr float MaxJitterDelay = 1.f;
}

UTRVoiceSubsystem* UTRVoiceSettingsWidget::GetVoice() const
{
	return UTRVoiceSubsystem::Get(this);
}

void UTRVoiceSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Check_PushToTalk != nullptr)
	{
		Check_PushToTalk->OnCheckStateChanged.AddDynamic(this, &UTRVoiceSettingsWidget::HandlePushToTalkChanged);
	}
	if (Check_OutputMuted != nullptr)
	{
		Check_OutputMuted->OnCheckStateChanged.AddDynamic(this, &UTRVoiceSettingsWidget::HandleOutputMutedChanged);
	}
	if (Slider_MicGain != nullptr)
	{
		Slider_MicGain->OnValueChanged.AddDynamic(this, &UTRVoiceSettingsWidget::HandleMicGainChanged);
	}
	if (Slider_MicThreshold != nullptr)
	{
		Slider_MicThreshold->OnValueChanged.AddDynamic(this, &UTRVoiceSettingsWidget::HandleMicThresholdChanged);
	}
	if (Slider_MicNoiseGate != nullptr)
	{
		Slider_MicNoiseGate->OnValueChanged.AddDynamic(this, &UTRVoiceSettingsWidget::HandleMicNoiseGateChanged);
	}
	if (Slider_JitterDelay != nullptr)
	{
		Slider_JitterDelay->OnValueChanged.AddDynamic(this, &UTRVoiceSettingsWidget::HandleJitterDelayChanged);
	}
	if (Button_Save != nullptr)
	{
		Button_Save->OnClicked.AddDynamic(this, &UTRVoiceSettingsWidget::HandleSaveClicked);
	}
	if (Button_Reset != nullptr)
	{
		Button_Reset->OnClicked.AddDynamic(this, &UTRVoiceSettingsWidget::HandleResetClicked);
	}
	if (Button_Close != nullptr)
	{
		Button_Close->OnClicked.AddDynamic(this, &UTRVoiceSettingsWidget::HandleCloseClicked);
	}

	// 값 변경과 발화 상태 모두 서브시스템이 알려 준다. 폴링하지 않는다.
	if (UTRVoiceSubsystem* Voice = GetVoice())
	{
		Voice->OnVoiceSettingsChanged.AddDynamic(this, &UTRVoiceSettingsWidget::RefreshFromSettings);
		Voice->OnTalkingChanged.AddDynamic(this, &UTRVoiceSettingsWidget::HandleTalkingChanged);
	}

	RefreshFromSettings();
}

void UTRVoiceSettingsWidget::NativeDestruct()
{
	if (UTRVoiceSubsystem* Voice = GetVoice())
	{
		Voice->OnVoiceSettingsChanged.RemoveDynamic(this, &UTRVoiceSettingsWidget::RefreshFromSettings);
		Voice->OnTalkingChanged.RemoveDynamic(this, &UTRVoiceSettingsWidget::HandleTalkingChanged);
	}

	Super::NativeDestruct();
}

void UTRVoiceSettingsWidget::HandleTalkingChanged(const FString& PlayerId, bool bIsTalking)
{
	RefreshStatus();
}

void UTRVoiceSettingsWidget::RefreshFromSettings()
{
	const UTRVoiceSubsystem* Voice = GetVoice();
	if (Voice == nullptr)
	{
		return;
	}

	TGuardValue<bool> Guard(bRefreshing, true);

	if (Check_PushToTalk != nullptr)
	{
		Check_PushToTalk->SetIsChecked(Voice->IsPushToTalk());
	}
	if (Check_OutputMuted != nullptr)
	{
		Check_OutputMuted->SetIsChecked(Voice->IsOutputMuted());
	}
	if (Slider_MicGain != nullptr)
	{
		Slider_MicGain->SetValue(Voice->GetMicGain() / TRVoiceUI::MaxMicGain);
	}
	if (Slider_MicThreshold != nullptr)
	{
		Slider_MicThreshold->SetValue(Voice->GetMicThreshold());
	}
	if (Slider_MicNoiseGate != nullptr)
	{
		Slider_MicNoiseGate->SetValue(Voice->GetMicNoiseGate());
	}
	if (Slider_JitterDelay != nullptr)
	{
		Slider_JitterDelay->SetValue(Voice->GetJitterDelay() / TRVoiceUI::MaxJitterDelay);
	}

	if (Text_MicGain != nullptr)
	{
		Text_MicGain->SetText(FText::AsNumber(Voice->GetMicGain()));
	}
	if (Text_MicThreshold != nullptr)
	{
		Text_MicThreshold->SetText(FText::AsNumber(Voice->GetMicThreshold()));
	}
	if (Text_MicNoiseGate != nullptr)
	{
		Text_MicNoiseGate->SetText(FText::AsNumber(Voice->GetMicNoiseGate()));
	}
	if (Text_JitterDelay != nullptr)
	{
		Text_JitterDelay->SetText(FText::AsNumber(Voice->GetJitterDelay()));
	}

	RefreshStatus();
}

void UTRVoiceSettingsWidget::RefreshStatus()
{
	const UTRVoiceSubsystem* Voice = GetVoice();
	if (Text_Status != nullptr && Voice != nullptr)
	{
		Text_Status->SetText(FText::FromString(Voice->GetStatusText()));
	}
}

void UTRVoiceSettingsWidget::HandlePushToTalkChanged(bool bIsChecked)
{
	if (!bRefreshing)
	{
		if (UTRVoiceSubsystem* Voice = GetVoice())
		{
			Voice->SetPushToTalk(bIsChecked);
		}
	}
}

void UTRVoiceSettingsWidget::HandleOutputMutedChanged(bool bIsChecked)
{
	if (!bRefreshing)
	{
		if (UTRVoiceSubsystem* Voice = GetVoice())
		{
			Voice->SetOutputMuted(bIsChecked);
		}
	}
}

void UTRVoiceSettingsWidget::HandleMicGainChanged(float Value)
{
	if (!bRefreshing)
	{
		if (UTRVoiceSubsystem* Voice = GetVoice())
		{
			Voice->SetMicGain(Value * TRVoiceUI::MaxMicGain);
		}
	}
}

void UTRVoiceSettingsWidget::HandleMicThresholdChanged(float Value)
{
	if (!bRefreshing)
	{
		if (UTRVoiceSubsystem* Voice = GetVoice())
		{
			Voice->SetMicThreshold(Value);
		}
	}
}

void UTRVoiceSettingsWidget::HandleMicNoiseGateChanged(float Value)
{
	if (!bRefreshing)
	{
		if (UTRVoiceSubsystem* Voice = GetVoice())
		{
			Voice->SetMicNoiseGate(Value);
		}
	}
}

void UTRVoiceSettingsWidget::HandleJitterDelayChanged(float Value)
{
	if (!bRefreshing)
	{
		if (UTRVoiceSubsystem* Voice = GetVoice())
		{
			Voice->SetJitterDelay(Value * TRVoiceUI::MaxJitterDelay);
		}
	}
}

void UTRVoiceSettingsWidget::HandleSaveClicked()
{
	if (UTRVoiceSubsystem* Voice = GetVoice())
	{
		Voice->SaveVoiceSettings();
	}
}

void UTRVoiceSettingsWidget::HandleResetClicked()
{
	if (UTRVoiceSubsystem* Voice = GetVoice())
	{
		Voice->ResetVoiceSettings();
	}
}

void UTRVoiceSettingsWidget::HandleCloseClicked()
{
	if (UTRVoiceSubsystem* Voice = GetVoice())
	{
		Voice->SetSettingsUIVisible(false);
	}
}

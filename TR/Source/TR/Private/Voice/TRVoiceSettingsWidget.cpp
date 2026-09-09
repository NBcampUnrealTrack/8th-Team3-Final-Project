// TRVoiceSettingsWidget.cpp

#include "Voice/TRVoiceSettingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Voice/TRVoiceSubsystem.h"

namespace TRVoiceUI
{
	// 슬라이더는 0~1 이라 실제 범위로 환산해서 쓴다.
	static constexpr float MaxMicGain = 2.f;
	static constexpr float MaxJitterDelay = 1.f;
	static constexpr float MaxVolume = 2.f;

	// 게이지가 튀지 않게 이 속도로 따라간다.
	static constexpr float GaugeFollowSpeed = 14.f;

	// 접속자 집합은 이 간격으로만 다시 본다.
	static constexpr float ListCheckInterval = 1.f;

	static FText VolumeText(float Value)
	{
		FNumberFormattingOptions Options;
		Options.SetMaximumFractionalDigits(2);
		return FText::AsNumber(Value, &Options);
	}

	static void SetFill(UWidget* Widget, float Weight)
	{
		if (UHorizontalBoxSlot* BoxSlot = Widget ? Cast<UHorizontalBoxSlot>(Widget->Slot) : nullptr)
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetVerticalAlignment(VAlign_Center);
			BoxSlot->SetPadding(FMargin(4.f, 0.f));
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = Weight;
			BoxSlot->SetSize(Size);
		}
	}
}

//--- 접속자 줄 ---

void UTRVoicePlayerRow::HandleVolumeChanged(float Value)
{
	if (UTRVoiceSubsystem* Voice = UTRVoiceSubsystem::Get(this))
	{
		Voice->SetPlayerVolume(NetId, Value * TRVoiceUI::MaxVolume);
	}
	if (Text_Volume != nullptr)
	{
		Text_Volume->SetText(TRVoiceUI::VolumeText(Value * TRVoiceUI::MaxVolume));
	}
}

//--- 패널 ---

UTRVoiceSettingsWidget::UTRVoiceSettingsWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 틱 빈도 Auto 는 C++ 부모를 가진 위젯의 NativeTick 을 그대로 돌린다. 따로 만질 것이 없다.
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
	if (Slider_OutputVolume != nullptr)
	{
		Slider_OutputVolume->OnValueChanged.AddDynamic(this, &UTRVoiceSettingsWidget::HandleOutputVolumeChanged);
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

	FixRowSlots();
	RefreshFromSettings();
	RebuildPlayerRows();
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

void UTRVoiceSettingsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateGauges(InDeltaTime);

	ListCheckAccum += InDeltaTime;
	if (ListCheckAccum >= TRVoiceUI::ListCheckInterval)
	{
		ListCheckAccum = 0.f;
		RebuildPlayerRows();
	}
}

void UTRVoiceSettingsWidget::FixRowSlots()
{
	TRVoiceUI::SetFill(Bar_MicLevel, 0.65f);
	TRVoiceUI::SetFill(Text_OutputVolume, 0.3f);

	// 접속자 줄과 상태 텍스트로 높이가 늘어난다. 배경 패널이 내용에 맞춰 자라게 한다.
	if (UWidget* Panel = GetWidgetFromName(TEXT("Panel")))
	{
		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(Panel->Slot))
		{
			PanelSlot->SetAutoSize(true);
		}
	}
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
	if (Slider_OutputVolume != nullptr)
	{
		Slider_OutputVolume->SetValue(Voice->GetOutputVolume() / TRVoiceUI::MaxVolume);
	}

	if (Text_MicGain != nullptr)
	{
		Text_MicGain->SetText(TRVoiceUI::VolumeText(Voice->GetMicGain()));
	}
	if (Text_MicThreshold != nullptr)
	{
		Text_MicThreshold->SetText(TRVoiceUI::VolumeText(Voice->GetMicThreshold()));
	}
	if (Text_MicNoiseGate != nullptr)
	{
		Text_MicNoiseGate->SetText(TRVoiceUI::VolumeText(Voice->GetMicNoiseGate()));
	}
	if (Text_JitterDelay != nullptr)
	{
		Text_JitterDelay->SetText(TRVoiceUI::VolumeText(Voice->GetJitterDelay()));
	}
	if (Text_OutputVolume != nullptr)
	{
		Text_OutputVolume->SetText(TRVoiceUI::VolumeText(Voice->GetOutputVolume()));
	}

	// 개인 음량이 밖에서 바뀌었을 수 있다(콘솔, 기본값 복귀).
	for (UTRVoicePlayerRow* Row : PlayerRows)
	{
		if (Row != nullptr && Row->Slider_Volume != nullptr)
		{
			const float Volume = Voice->GetPlayerVolume(Row->NetId);
			Row->Slider_Volume->SetValue(Volume / TRVoiceUI::MaxVolume);
			if (Row->Text_Volume != nullptr)
			{
				Row->Text_Volume->SetText(TRVoiceUI::VolumeText(Volume));
			}
		}
	}

	RefreshStatus();
}

void UTRVoiceSettingsWidget::RefreshStatus()
{
	const UTRVoiceSubsystem* Voice = GetVoice();
	if (Text_Status != nullptr && Voice != nullptr)
	{
		Text_Status->SetText(FText::FromString(Voice->GetStatusText(/*bIncludeEngineDump=*/false)));
	}
}

void UTRVoiceSettingsWidget::UpdateGauges(float DeltaTime)
{
	UTRVoiceSubsystem* Voice = GetVoice();
	if (Voice == nullptr)
	{
		return;
	}

	if (Bar_MicLevel != nullptr)
	{
		ShownMicLevel = FMath::FInterpTo(ShownMicLevel, Voice->GetLocalMicLevel(), DeltaTime, TRVoiceUI::GaugeFollowSpeed);
		Bar_MicLevel->SetPercent(ShownMicLevel);
	}

	if (PlayerRows.Num() == 0)
	{
		return;
	}

	const TArray<FTRVoicePlayerInfo> Players = Voice->GetRemotePlayers();
	for (UTRVoicePlayerRow* Row : PlayerRows)
	{
		if (Row == nullptr || Row->Bar_Level == nullptr)
		{
			continue;
		}

		const FTRVoicePlayerInfo* Info = Players.FindByPredicate([Row](const FTRVoicePlayerInfo& P)
		{
			return P.NetId == Row->NetId;
		});

		const float Target = (Info != nullptr && Info->bIsTalking) ? FMath::Max(Info->Level, 0.15f) : (Info ? Info->Level : 0.f);
		Row->ShownLevel = FMath::FInterpTo(Row->ShownLevel, Target, DeltaTime, TRVoiceUI::GaugeFollowSpeed);
		Row->Bar_Level->SetPercent(Row->ShownLevel);
	}
}

void UTRVoiceSettingsWidget::RebuildPlayerRows()
{
	UTRVoiceSubsystem* Voice = GetVoice();
	if (Box_Players == nullptr || Voice == nullptr || WidgetTree == nullptr)
	{
		return;
	}

	const TArray<FTRVoicePlayerInfo> Players = Voice->GetRemotePlayers();

	// 집합이 같으면 줄을 다시 만들지 않는다. 슬라이더를 잡고 있는 손이 끊긴다.
	bool bSame = (Players.Num() == PlayerRows.Num());
	for (int32 Index = 0; bSame && Index < Players.Num(); ++Index)
	{
		bSame = (PlayerRows[Index] != nullptr && PlayerRows[Index]->NetId == Players[Index].NetId);
	}
	if (bSame)
	{
		return;
	}

	Box_Players->ClearChildren();
	PlayerRows.Reset();

	for (const FTRVoicePlayerInfo& Player : Players)
	{
		UTRVoicePlayerRow* Row = NewObject<UTRVoicePlayerRow>(this);
		Row->NetId = Player.NetId;

		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();

		Row->Text_Name = WidgetTree->ConstructWidget<UTextBlock>();
		Row->Text_Name->SetText(FText::FromString(Player.DisplayName));
		Line->AddChildToHorizontalBox(Row->Text_Name);
		TRVoiceUI::SetFill(Row->Text_Name, 0.35f);

		Row->Bar_Level = WidgetTree->ConstructWidget<UProgressBar>();
		Row->Bar_Level->SetPercent(0.f);
		Line->AddChildToHorizontalBox(Row->Bar_Level);
		TRVoiceUI::SetFill(Row->Bar_Level, 0.2f);

		Row->Slider_Volume = WidgetTree->ConstructWidget<USlider>();
		Row->Slider_Volume->SetValue(Player.Volume / TRVoiceUI::MaxVolume);
		Row->Slider_Volume->OnValueChanged.AddDynamic(Row, &UTRVoicePlayerRow::HandleVolumeChanged);
		Line->AddChildToHorizontalBox(Row->Slider_Volume);
		TRVoiceUI::SetFill(Row->Slider_Volume, 0.35f);

		Row->Text_Volume = WidgetTree->ConstructWidget<UTextBlock>();
		Row->Text_Volume->SetText(TRVoiceUI::VolumeText(Player.Volume));
		Line->AddChildToHorizontalBox(Row->Text_Volume);
		TRVoiceUI::SetFill(Row->Text_Volume, 0.1f);

		if (UVerticalBoxSlot* LineSlot = Box_Players->AddChildToVerticalBox(Line))
		{
			LineSlot->SetPadding(FMargin(0.f, 2.f));
		}

		PlayerRows.Add(Row);
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

void UTRVoiceSettingsWidget::HandleOutputVolumeChanged(float Value)
{
	if (!bRefreshing)
	{
		if (UTRVoiceSubsystem* Voice = GetVoice())
		{
			Voice->SetOutputVolume(Value * TRVoiceUI::MaxVolume);
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

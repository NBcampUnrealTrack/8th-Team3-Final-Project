#include "UI/Widget/BGSystemMenuWidget.h"

#include "Audio/BGAudioSubsystem.h"
#include "BGGameplayTags.h"
#include "Components/InputComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Card/BGGameDataTypes.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "GenericPlatform/GenericApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UnrealEngine.h"
#include "Session/BGSessionFlow.h"
#include "Session/BGSessionInterface.h"
#include "Widgets/SWindow.h"
#include "Core/BGGameInstance.h"
#include "Core/BGGameMode.h"
#include "Core/BGPlayerController.h"
#include "UI/Target/BGWorldTargetingComponent.h"
#include "UI/View/BGInfoTooltipRow.h"
#include "UI/Widget/BGChatingWidget.h"
#include "UI/Widget/BGHelpRowWidget.h"

namespace
{
	// 콤보 목록의 순서와 그대로 짝이 된다.
	const TArray<EWindowMode::Type> WindowModeValues = {
		EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };

	const TCHAR* WindowModeLabels[] = { TEXT("전체 화면"), TEXT("전체 창 모드"), TEXT("창 모드") };

	const TCHAR* QualityLabels[] = { TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("매우 높음") };

	// 항목마다 품질이 제각각이면 GetOverallScalabilityLevel 이 -1 을 준다. 기본값 복원 뒤에도
	// 그렇게 되는 경우가 있어, 고를 수 없는 마지막 칸을 하나 두고 거기에 세운다.
	// (이게 없으면 콤보가 빈칸으로 남는다.)
	const TCHAR* CustomQualityLabel = TEXT("사용자 지정");
	const int32 CustomQualityIndex = UE_ARRAY_COUNT(QualityLabels);

	// 세부 품질 표. 순서가 UBGSystemMenuWidget::DetailCombos 와 같아야 한다.
	// 실내 테이블 씬이라 시야 거리·폴리지·랜드스케이프는 차이가 없어 넣지 않았다.
	using FQualitySetter = void (UGameUserSettings::*)(int32);
	using FQualityGetter = int32 (UGameUserSettings::*)() const;

	const FQualitySetter DetailSetters[] = {
		&UGameUserSettings::SetShadowQuality,
		&UGameUserSettings::SetTextureQuality,
		&UGameUserSettings::SetVisualEffectQuality,
		&UGameUserSettings::SetAntiAliasingQuality,
		&UGameUserSettings::SetPostProcessingQuality,
		&UGameUserSettings::SetGlobalIlluminationQuality };

	const FQualityGetter DetailGetters[] = {
		&UGameUserSettings::GetShadowQuality,
		&UGameUserSettings::GetTextureQuality,
		&UGameUserSettings::GetVisualEffectQuality,
		&UGameUserSettings::GetAntiAliasingQuality,
		&UGameUserSettings::GetPostProcessingQuality,
		&UGameUserSettings::GetGlobalIlluminationQuality };

	// 0 = 무제한.
	const TArray<float> FrameLimitValues = { 0.f, 30.f, 60.f, 120.f, 144.f, 240.f };

	// 구분선이 화면에서 차지해야 할 픽셀 두께. DPI 스케일과 무관하게 이 값으로 떨어진다.
	constexpr float SystemMenuDividerPixels = 2.f;

	FString ResolutionLabel(const FIntPoint& Resolution)
	{
		return FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
	}

	FString FrameLimitLabel(float Limit)
	{
		return (Limit <= 0.f) ? FString(TEXT("무제한")) : FString::Printf(TEXT("%d"), FMath::RoundToInt(Limit));
	}

	// 게임 창의 화면상 중심. 창을 못 찾으면 (MAX,MAX) 로 둬서 어느 모니터에도 안 걸리게 한다.
	FIntPoint GetGameWindowCenter()
	{
		if (GEngine && GEngine->GameViewport)
		{
			if (const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow())
			{
				const FVector2D Position = Window->GetPositionInScreen();
				const FVector2D Size = Window->GetSizeInScreen();
				return FIntPoint(FMath::RoundToInt(Position.X + Size.X * 0.5),
				                 FMath::RoundToInt(Position.Y + Size.Y * 0.5));
			}
		}
		return FIntPoint(MAX_int32, MAX_int32);
	}

	// ─ 도움말 ─

	// 읽는 순서는 진영 순서다. 태그를 코드가 갖고 있으므로 DT 의 행 순서에 맡기지 않는다.
	const FGameplayTag* HelpRoleOrder()
	{
		static const FGameplayTag Tags[] = {
			BG::Role::Sheriff.GetTag(), BG::Role::Deputy.GetTag(),
			BG::Role::Outlaw.GetTag(),  BG::Role::Renegade.GetTag() };
		return Tags;
	}
	constexpr int32 HelpRoleCount = 4;

	// 직업 이름. DT 의 DisplayName 이 우선이고, 비어 있으면 태그의 마지막 마디를 쓴다
	// (Character.Job.Mage → "Mage"). 폴백이 보기 나빠도 그대로 둔다 — DT 를 채우라는 신호다.
	FText HelpJobTitle(const FBGCharacterRow& Row)
	{
		if (!Row.DisplayName.IsEmpty())
		{
			return Row.DisplayName;
		}

		const FString Tag = Row.CharacterTag.ToString();
		FString Last;
		return Tag.Split(TEXT("."), nullptr, &Last, ESearchCase::IgnoreCase, ESearchDir::FromEnd)
			? FText::FromString(Last)
			: FText::FromString(Tag);
	}

	UTexture2D* FindHelpIcon(const TMap<FGameplayTag, TObjectPtr<UTexture2D>>& Table, const FGameplayTag& Tag)
	{
		const TObjectPtr<UTexture2D>* Found = Tag.IsValid() ? Table.Find(Tag) : nullptr;
		return Found ? Found->Get() : nullptr;
	}
}

FIntPoint UBGSystemMenuWidget::GetCurrentMonitorResolution()
{
	// 캐시가 아니라 새로 만든다 — 모니터를 옮긴 뒤에도 지금 배치가 나와야 한다.
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);

	const FIntPoint Center = GetGameWindowCenter();
	const FMonitorInfo* Fallback = nullptr;

	for (const FMonitorInfo& Monitor : Metrics.MonitorInfo)
	{
		if (Monitor.bIsPrimary || !Fallback)
		{
			Fallback = &Monitor;
		}

		const FPlatformRect& Rect = Monitor.DisplayRect;
		if (Center.X >= Rect.Left && Center.X < Rect.Right
			&& Center.Y >= Rect.Top && Center.Y < Rect.Bottom)
		{
			return FIntPoint(Monitor.NativeWidth, Monitor.NativeHeight);
		}
	}

	return Fallback ? FIntPoint(Fallback->NativeWidth, Fallback->NativeHeight) : FIntPoint::ZeroValue;
}

int32 UBGSystemMenuWidget::FindClosestResolutionIndex(const FIntPoint& Target) const
{
	int32 Closest = INDEX_NONE;
	int64 BestDelta = MAX_int64;

	for (int32 Index = 0; Index < ResolutionOptions.Num(); ++Index)
	{
		const FIntPoint& Option = ResolutionOptions[Index];
		const int64 Delta = FMath::Abs(static_cast<int64>(Option.X) * Option.Y
			- static_cast<int64>(Target.X) * Target.Y);
		if (Delta < BestDelta)
		{
			BestDelta = Delta;
			Closest = Index;
		}
	}
	return Closest;
}

void UBGSystemMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->BindDefaultButtonsInWidget(this);
	}

	if (SettingsButton) { SettingsButton->OnClicked.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleSettingsClicked); }
	if (CloseButton)    { CloseButton->OnClicked.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleCloseClicked); }
	if (ResetButton)    { ResetButton->OnClicked.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleResetClicked); }
	if (MainMenuButton) { MainMenuButton->OnClicked.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleMainMenuClicked); }

	if (HelpButton)      { HelpButton->OnClicked.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleHelpClicked); }
	if (HelpCloseButton) { HelpCloseButton->OnClicked.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleHelpCloseClicked); }
	if (Tab_Jobs)        { Tab_Jobs->OnClicked.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleJobsTabClicked); }
	if (Tab_Roles)       { Tab_Roles->OnClicked.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleRolesTabClicked); }
	if (Tab_Equips)      { Tab_Equips->OnClicked.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleEquipsTabClicked); }

	if (WindowModeCombo)  { WindowModeCombo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleWindowModeChanged); }
	if (ResolutionCombo)
	{
		ResolutionCombo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleResolutionChanged);
		ResolutionCombo->OnOpening.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleResolutionComboOpening);
	}
	if (QualityCombo)     { QualityCombo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleQualityChanged); }
	if (FrameLimitCombo)  { FrameLimitCombo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleFrameLimitChanged); }
	if (VSyncCheck)       { VSyncCheck->OnCheckStateChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleVSyncChanged); }
	if (DoubleSpeedCheck) { DoubleSpeedCheck->OnCheckStateChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleDoubleSpeedChanged); }
	RefreshDoubleSpeedCheck();

	// 세부 품질 — 선언 순서 그대로 담고, 같은 자리의 게터/세터와 짝이 된다.
	DetailCombos = { ShadowCombo, TextureCombo, EffectsCombo,
	                 AntiAliasingCombo, PostProcessCombo, GlobalIlluminationCombo };

	if (ShadowCombo)             { ShadowCombo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleShadowChanged); }
	if (TextureCombo)            { TextureCombo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleTextureChanged); }
	if (EffectsCombo)            { EffectsCombo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleEffectsChanged); }
	if (AntiAliasingCombo)       { AntiAliasingCombo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleAntiAliasingChanged); }
	if (PostProcessCombo)        { PostProcessCombo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandlePostProcessChanged); }
	if (GlobalIlluminationCombo) { GlobalIlluminationCombo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleGlobalIlluminationChanged); }

	AudioCombos = { WindowModeCombo, ResolutionCombo, QualityCombo, FrameLimitCombo,
	                ShadowCombo, TextureCombo, EffectsCombo, AntiAliasingCombo,
	                PostProcessCombo, GlobalIlluminationCombo };
	for (UComboBoxString* Combo : AudioCombos)
	{
		if (!Combo)
		{
			continue;
		}
		Combo->OnOpening.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleComboOpeningAudio);
		Combo->OnSelectionChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleComboSelectionAudio);
	}

	if (MasterVolumeSlider) { MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleMasterVolumeChanged); }
	if (MusicVolumeSlider)  { MusicVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleMusicVolumeChanged); }
	if (SFXVolumeSlider)    { SFXVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleSFXVolumeChanged); }

	AudioSliders = { MasterVolumeSlider, MusicVolumeSlider, SFXVolumeSlider };
	for (USlider* Slider : AudioSliders)
	{
		if (!Slider)
		{
			continue;
		}
		Slider->OnMouseCaptureBegin.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleSliderCaptureBeginAudio);
		Slider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleSliderCaptureEndAudio);
		Slider->OnControllerCaptureBegin.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleSliderCaptureBeginAudio);
		Slider->OnControllerCaptureEnd.AddUniqueDynamic(this, &UBGSystemMenuWidget::HandleSliderCaptureEndAudio);
	}

	Dividers = { Image_Div1, Image_Div2, Image_Div3, Image_Div4, Help_Div1, Help_Div2 };
	RefreshDividerThickness();

	RefreshVolumesFromAudioSubsystem();

	SyncSettingsToActualWindow();
	PopulateOptions();
	RefreshFromSettings();

	// 도움말 문구는 판이 돌아가는 동안 바뀌지 않는다 — 한 번 만들어 두고 열 때는 보이기만 한다.
	BuildHelpRows();
	SelectHelpTab(0);

	TryBindToggleInput();

	// 패널은 버튼이나 ESC 를 눌러야 열린다.
	if (SettingsPanel)
	{
		SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (HelpPanel)
	{
		HelpPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBGSystemMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 창 크기·해상도가 바뀌면 DPI 스케일도 바뀐다. 값이 달라졌을 때만 다시 맞춘다.
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (!FMath::IsNearlyEqual(Scale, LastViewportScale))
	{
		RefreshDividerThickness();
	}

	PopupAnim.Tick(InDeltaTime);
	RefreshInteractiveAudioHover();
}

void UBGSystemMenuWidget::RefreshInteractiveAudioHover()
{
	UWidget* NewHovered = nullptr;
	for (UComboBoxString* Combo : AudioCombos)
	{
		if (Combo && Combo->IsVisible() && Combo->GetIsEnabled() && Combo->IsHovered())
		{
			NewHovered = Combo;
			break;
		}
	}

	if (!NewHovered)
	{
		for (USlider* Slider : AudioSliders)
		{
			if (Slider && Slider->IsVisible() && Slider->GetIsEnabled() && Slider->IsHovered())
			{
				NewHovered = Slider;
				break;
			}
		}
	}

	UWidget* OldHovered = HoveredAudioWidget.Get();
	if (OldHovered == NewHovered)
	{
		return;
	}

	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		if (Cast<UComboBoxString>(OldHovered))
		{
			Audio->Play(BG::Audio::UI::ComboBox::HoverEnd.GetTag());
		}
		else if (Cast<USlider>(OldHovered))
		{
			Audio->Play(BG::Audio::UI::Slider::HoverEnd.GetTag());
		}

		if (Cast<UComboBoxString>(NewHovered))
		{
			Audio->Play(BG::Audio::UI::ComboBox::HoverBegin.GetTag());
		}
		else if (Cast<USlider>(NewHovered))
		{
			Audio->Play(BG::Audio::UI::Slider::HoverBegin.GetTag());
		}
	}

	HoveredAudioWidget = NewHovered;
}

void UBGSystemMenuWidget::HandleComboOpeningAudio()
{
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->Play(BG::Audio::UI::ComboBox::Open.GetTag());
	}
}

void UBGSystemMenuWidget::HandleComboSelectionAudio(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// SetSelectedIndex 등 코드가 현재 표시를 맞추는 Direct 변경에는 선택음을 내지 않는다.
	if (bRefreshing || SelectionType == ESelectInfo::Direct)
	{
		return;
	}

	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->Play(BG::Audio::UI::ComboBox::Select.GetTag());
	}
}

void UBGSystemMenuWidget::HandleSliderCaptureBeginAudio()
{
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->Play(BG::Audio::UI::Slider::CaptureBegin.GetTag());
	}
}

void UBGSystemMenuWidget::HandleSliderCaptureEndAudio()
{
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->Play(BG::Audio::UI::Slider::CaptureEnd.GetTag());
	}
}

void UBGSystemMenuWidget::RefreshDividerThickness()
{
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (Scale <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	LastViewportScale = Scale;

	//	스케일이 곱해진 뒤 정확히 SystemMenuDividerPixels 가 되도록 미리 나눠 둔다.
	const float Thickness = SystemMenuDividerPixels / Scale;
	for (UImage* Divider : Dividers)
	{
		if (!Divider)
		{
			continue;
		}
		FSlateBrush Brush = Divider->GetBrush();
		Brush.ImageSize.Y = Thickness;
		Divider->SetBrush(Brush);
	}
}

void UBGSystemMenuWidget::NativeDestruct()
{
	HoveredAudioWidget.Reset();
	AudioCombos.Reset();
	AudioSliders.Reset();
	UnbindToggleInput();
	Super::NativeDestruct();
}

void UBGSystemMenuWidget::TryBindToggleInput()
{
	APlayerController* PC = GetOwningPlayer();
	if (bToggleInputBound || !PC || !PC->InputComponent)
	{
		return;
	}

	// ESC 는 PlayerController 가 대상 선택 취소에 이미 물려 두었다. 키를 뺏는 게 아니라
	// 한 번 더 붙는 것이라 두 핸들러가 같이 불린다 — 양보는 HandleTogglePressed 에서 한다.
	PC->InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &UBGSystemMenuWidget::HandleTogglePressed);
	bToggleInputBound = true;
}

void UBGSystemMenuWidget::UnbindToggleInput()
{
	APlayerController* PC = GetOwningPlayer();
	if (!bToggleInputBound || !PC || !PC->InputComponent)
	{
		bToggleInputBound = false;
		return;
	}

	// HUD 가 다시 만들어질 때마다 바인딩이 쌓이지 않게 내 것만 걷어낸다.
	PC->InputComponent->KeyBindings.RemoveAll([this](const FInputKeyBinding& Binding)
	{
		return Binding.KeyDelegate.GetUObject() == this;
	});
	bToggleInputBound = false;
}

void UBGSystemMenuWidget::HandleTogglePressed()
{
	// 도움말이 떠 있으면 ESC 는 그것부터 닫는다. 설정을 여는 건 그다음 ESC 다.
	if (IsHelpOpen())
	{
		CloseHelp();
		return;
	}

	if (IsSettingsOpen())
	{
		CloseSettings();
		return;
	}

	// 여는 쪽만 양보한다 — 조준 취소가 ESC 의 원래 주인이고, 채팅 입력 중에도 끼어들지 않는다.
	// 게임 PC 가 아니면(메인메뉴) 양보할 상대가 없으므로 그대로 연다.
	if (const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwningPlayer()))
	{
		const UBGWorldTargetingComponent* Targeting = PC->FindComponentByClass<UBGWorldTargetingComponent>();
		if (Targeting && Targeting->IsTargeting())
		{
			return;
		}

		if (PC->ChatWidgetInstance && PC->ChatWidgetInstance->IsInputOpen())
		{
			return;
		}
	}

	OpenSettings();
}

void UBGSystemMenuWidget::PopulateOptions()
{
	TGuardValue<bool> Guard(bRefreshing, true);

	if (WindowModeCombo)
	{
		WindowModeCombo->ClearOptions();
		for (const TCHAR* Label : WindowModeLabels)
		{
			WindowModeCombo->AddOption(Label);
		}
	}

	if (ResolutionCombo)
	{
		ResolutionCombo->ClearOptions();
		ResolutionOptions.Reset();

		// GetSupportedFullscreenResolutions 는 RHIGetAvailableResolutions 를 부르는데,
		// 그건 주 어댑터 기준이라 창을 다른 모니터로 옮겨도 그대로다. 지금 창이 올라가 있는
		// 모니터를 찾아 그 네이티브 해상도로 잘라내고, 그 값 자체도 목록에 넣는다.
		const FIntPoint Native = GetCurrentMonitorResolution();
		PopulatedMonitorResolution = Native;

		TArray<FIntPoint> Supported;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
		for (const FIntPoint& Resolution : Supported)
		{
			if (Native.X <= 0 || (Resolution.X <= Native.X && Resolution.Y <= Native.Y))
			{
				ResolutionOptions.AddUnique(Resolution);
			}
		}

		if (Native.X > 0 && Native.Y > 0)
		{
			ResolutionOptions.AddUnique(Native);
		}

		// 현재 해상도는 목록에 넣지 않는다 — 창 모드에서는 테두리를 뺀 창 크기(1914x1040 같은)
		// 가 잡혀 있어서, 그걸 항목으로 만들면 고를 수 있는 가짜 해상도가 하나 생긴다.
		ResolutionOptions.Sort([](const FIntPoint& A, const FIntPoint& B)
		{
			return (A.X * A.Y) < (B.X * B.Y);
		});

		for (const FIntPoint& Resolution : ResolutionOptions)
		{
			ResolutionCombo->AddOption(ResolutionLabel(Resolution));
		}
	}

	if (QualityCombo)
	{
		QualityCombo->ClearOptions();
		for (const TCHAR* Label : QualityLabels)
		{
			QualityCombo->AddOption(Label);
		}
		QualityCombo->AddOption(CustomQualityLabel);
	}

	// 세부는 프리셋 네 칸뿐이다 — 항상 한 칸으로 떨어지므로 "사용자 지정" 이 필요 없다.
	for (UComboBoxString* Combo : DetailCombos)
	{
		if (!Combo)
		{
			continue;
		}
		Combo->ClearOptions();
		for (const TCHAR* Label : QualityLabels)
		{
			Combo->AddOption(Label);
		}
	}

	if (FrameLimitCombo)
	{
		FrameLimitCombo->ClearOptions();
		for (const float Limit : FrameLimitValues)
		{
			FrameLimitCombo->AddOption(FrameLimitLabel(Limit));
		}
	}
}

void UBGSystemMenuWidget::SyncSettingsToActualWindow()
{
	// PIE 창은 에디터 안의 뷰포트라 게임 창 설정으로 옮기면 저장 파일만 더럽힌다.
	if (GIsEditor)
	{
		return;
	}

	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const TSharedPtr<SWindow> Window = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWindow() : nullptr;
	if (!Settings || !Window.IsValid())
	{
		return;
	}

	Settings->SetFullscreenMode(Window->GetWindowMode());
	if (GSystemResolution.ResX > 0 && GSystemResolution.ResY > 0)
	{
		Settings->SetScreenResolution(FIntPoint(GSystemResolution.ResX, GSystemResolution.ResY));
	}
}

void UBGSystemMenuWidget::RefreshFromSettings()
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}

	TGuardValue<bool> Guard(bRefreshing, true);

	if (WindowModeCombo)
	{
		const int32 Index = WindowModeValues.IndexOfByKey(Settings->GetFullscreenMode());
		if (WindowModeValues.IsValidIndex(Index))
		{
			WindowModeCombo->SetSelectedIndex(Index);
		}
	}

	if (ResolutionCombo)
	{
		// 전체 창 모드는 엔진이 ResolutionSizeX/Y 를 아예 보지 않고 데스크톱 해상도를 쓴다
		// (GameUserSettings 의 해상도 계산이 전부 GetDesktopResolution 으로 갈라진다).
		// 고를 수 있게 두면 눌러도 아무 일이 안 일어나므로 잠그고 실제 값을 보여준다.
		const bool bLockedToDesktop = (Settings->GetFullscreenMode() == EWindowMode::WindowedFullscreen);
		ResolutionCombo->SetIsEnabled(!bLockedToDesktop);

		const FIntPoint Current = bLockedToDesktop
			? GetCurrentMonitorResolution() : Settings->GetScreenResolution();
		int32 Index = ResolutionOptions.IndexOfByKey(Current);
		if (Index == INDEX_NONE)
		{
			// 창을 끌어서 만든 크기는 목록에 없다. 빈칸으로 두지 않고 가장 가까운 칸을 세운다.
			Index = FindClosestResolutionIndex(Current);
		}
		if (ResolutionOptions.IsValidIndex(Index))
		{
			ResolutionCombo->SetSelectedIndex(Index);
		}
	}

	if (QualityCombo)
	{
		const int32 Level = Settings->GetOverallScalabilityLevel();
		QualityCombo->SetSelectedIndex(
			(Level >= 0 && Level < CustomQualityIndex) ? Level : CustomQualityIndex);
	}

	for (int32 Row = 0; Row < DetailCombos.Num(); ++Row)
	{
		if (UComboBoxString* Combo = DetailCombos[Row])
		{
			// 콘솔에서 sg.* 를 시네마틱(4)으로 올려둔 경우가 있어 프리셋 범위로 물린다.
			const int32 Level = (Settings->*DetailGetters[Row])();
			Combo->SetSelectedIndex(FMath::Clamp(Level, 0, CustomQualityIndex - 1));
		}
	}

	if (FrameLimitCombo)
	{
		const int32 Index = FrameLimitValues.IndexOfByPredicate([Limit = Settings->GetFrameRateLimit()](float Value)
		{
			return FMath::IsNearlyEqual(Value, Limit);
		});
		if (FrameLimitValues.IsValidIndex(Index))
		{
			FrameLimitCombo->SetSelectedIndex(Index);
		}
		else
		{
			FrameLimitCombo->ClearSelection();
		}
	}

	if (VSyncCheck)
	{
		VSyncCheck->SetIsChecked(Settings->IsVSyncEnabled());
	}

	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->SetValue(MasterVolume);
	}
	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->SetValue(MusicVolume);
	}
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->SetValue(SFXVolume);
	}
	RefreshVolumeTexts();
}

void UBGSystemMenuWidget::ApplyAndSaveGraphics()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
}

void UBGSystemMenuWidget::RefreshVolumesFromAudioSubsystem()
{
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		MasterVolume = Audio->GetMasterVolume();
		MusicVolume  = Audio->GetMusicVolume();
		SFXVolume    = Audio->GetSFXVolume();
		Audio->ApplyVolumes();
	}
}

void UBGSystemMenuWidget::RefreshVolumeTexts() const
{
	auto Write = [](UTextBlock* Label, float Volume)
	{
		if (Label)
		{
			Label->SetText(FText::FromString(
				FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Volume * 100.f))));
		}
	};

	Write(MasterVolumeText, MasterVolume);
	Write(MusicVolumeText, MusicVolume);
	Write(SFXVolumeText, SFXVolume);
}

void UBGSystemMenuWidget::OpenSettings()
{
	// 패널은 한 번에 하나만 띄운다.
	CloseHelp();

	// 창을 다른 모니터로 옮겼을 수 있다 — 열 때마다 해상도 목록부터 다시 만든다.
	SyncSettingsToActualWindow();
	PopulateOptions();
	RefreshFromSettings();
	if (SettingsPanel)
	{
		SettingsPanel->SetVisibility(ESlateVisibility::Visible);
	}
	PopupAnim.Start(Size_Popup);
}

void UBGSystemMenuWidget::SetStandaloneMode(bool bStandalone)
{
	const ESlateVisibility Mode = bStandalone ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	if (SettingsButton) { SettingsButton->SetVisibility(Mode); }
	if (MainMenuButton) { MainMenuButton->SetVisibility(Mode); }
	if (HelpButton)     { HelpButton->SetVisibility(Mode); }

	//	HUD 가 없는 곳에서는 2배속 토글도 의미가 없다. 돌아오면 PvE 여부로 다시 정한다.
	if (bStandalone)
	{
		if (DoubleSpeedCheck) { DoubleSpeedCheck->SetVisibility(ESlateVisibility::Collapsed); }
	}
	else
	{
		RefreshDoubleSpeedCheck();
	}
}

void UBGSystemMenuWidget::RefreshDoubleSpeedCheck()
{
	if (!DoubleSpeedCheck)
	{
		return;
	}

	const bool bPvE = ABGGameMode::GetPlayMode(this) == EBGPlayMode::PvE;
	DoubleSpeedCheck->SetVisibility(bPvE ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	//	체크 표시를 바꾸는 통지가 다시 RPC 를 보내지 않게 막는다.
	TGuardValue<bool> Guard(bRefreshing, true);
	//	GameInstance 값을 읽는다. 위젯이 GameState 복제보다 먼저 만들어져도 조우 사이에 선택이 유지된다.
	const UBGGameInstance* GI = Cast<UBGGameInstance>(GetGameInstance());
	DoubleSpeedCheck->SetIsChecked(IsValid(GI) && GI->IsPresentationDoubleSpeed());
}

void UBGSystemMenuWidget::HandleDoubleSpeedChanged(bool bIsChecked)
{
	if (bRefreshing)
	{
		return;
	}

	if (ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwningPlayer()); IsValid(PC))
	{
		PC->RequestSetPresentationDoubleSpeed(bIsChecked);
	}
}

void UBGSystemMenuWidget::CloseSettings()
{
	if (SettingsPanel)
	{
		SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBGSystemMenuWidget::HandleSettingsClicked()
{
	if (IsSettingsOpen())
	{
		CloseSettings();
	}
	else
	{
		OpenSettings();
	}
}

void UBGSystemMenuWidget::HandleCloseClicked()
{
	CloseSettings();
}

void UBGSystemMenuWidget::HandleMainMenuClicked()
{
	// 세션 파기 → 세션 메뉴 닫기 → 메인메뉴 맵 이동까지 LeaveSession 이 다 한다.
	// 세션 메뉴의 "나가기" 와 같은 경로라 여기서 따로 정리할 게 없다.
	UGameInstance* GI = GetGameInstance();
	if (IBGSessionInterface* Session = GI ? GI->GetSubsystem<UBGSessionFlow>() : nullptr)
	{
		Session->LeaveSession();
	}
}

void UBGSystemMenuWidget::HandleResetClicked()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetToDefaults();

		// SetToDefaults 는 품질을 에픽으로 세우면서 ResolutionQuality 만 0 으로 남긴다
		// (프로젝트 기본 스크린 퍼센티지로 넘기려는 것). 그런데 GetSingleQualityLevel 은
		// 해상도 품질까지 맞아야 레벨을 주므로 이 상태는 항상 -1 이고, 콤보가 "사용자 지정"
		// 으로 뜬다. SetDefaults 가 원래 노린 값인 마지막 프리셋(에픽)으로 다시 세운다.
		Settings->SetOverallScalabilityLevel(CustomQualityIndex - 1);

		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}

	MasterVolume = 1.f;
	MusicVolume  = 1.f;
	SFXVolume    = 1.f;
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->SetMasterVolume(MasterVolume);
		Audio->SetMusicVolume(MusicVolume);
		Audio->SetSFXVolume(SFXVolume);
	}

	// 기본 해상도 목록이 바뀔 수 있다 — 목록부터 다시 만든다.
	PopulateOptions();
	RefreshFromSettings();
}

void UBGSystemMenuWidget::HandleWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRefreshing || !WindowModeCombo)
	{
		return;
	}

	const int32 Index = WindowModeCombo->GetSelectedIndex();
	if (!WindowModeValues.IsValidIndex(Index))
	{
		return;
	}

	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetFullscreenMode(WindowModeValues[Index]);

		// 전체 창 모드는 엔진이 데스크톱 해상도로 그리지만 ResolutionSizeX/Y 는 옛 값을 그대로
		// 들고 있다. 그 상태로 전체 화면으로 돌아가면 옛 값이 되살아나 해상도가 뚝 떨어지고,
		// 콤보가 보여주던 값과도 어긋난다 — 모드를 바꿀 때 저장값도 같이 맞춰 둔다.
		if (WindowModeValues[Index] == EWindowMode::WindowedFullscreen)
		{
			const FIntPoint Native = GetCurrentMonitorResolution();
			if (Native.X > 0 && Native.Y > 0)
			{
				Settings->SetScreenResolution(Native);
			}
		}

		ApplyAndSaveGraphics();

		// 설정을 연 채로 창을 다른 모니터로 끌고 가서 모드를 바꿨을 수 있다. 목록은 열 때
		// 만들어진 것이라 옛 모니터 기준으로 잘려 있고, 새 모니터 해상도가 목록에 없으면
		// 표시가 가장 가까운 옛 값으로 스냅된다 — 목록부터 다시 만든 뒤에 칸을 맞춘다.
		// (전체 창 모드로 들어가고 나올 때의 잠금 상태도 여기서 같이 갱신된다.)
		PopulateOptions();
		RefreshFromSettings();
	}
}

void UBGSystemMenuWidget::HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRefreshing || !ResolutionCombo)
	{
		return;
	}

	const int32 Index = ResolutionCombo->GetSelectedIndex();
	if (!ResolutionOptions.IsValidIndex(Index))
	{
		return;
	}

	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetScreenResolution(ResolutionOptions[Index]);
		ApplyAndSaveGraphics();
	}
}

void UBGSystemMenuWidget::HandleResolutionComboOpening()
{
	// 목록은 OpenSettings 에서 만든다. 패널을 연 채로 창을 옮기면 그 뒤로는 다시 만들 기회가
	// 없어 옛 모니터 기준 목록이 그대로 펼쳐진다. 모니터가 같으면 손대지 않는다.
	if (GetCurrentMonitorResolution() == PopulatedMonitorResolution)
	{
		return;
	}

	SyncSettingsToActualWindow();
	PopulateOptions();
	RefreshFromSettings();
}

void UBGSystemMenuWidget::HandleQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRefreshing || !QualityCombo)
	{
		return;
	}

	// "사용자 지정" 은 보여주기만 하는 칸이다 — 골라도 아무것도 바꾸지 않는다.
	const int32 Index = QualityCombo->GetSelectedIndex();
	if (Index < 0 || Index >= CustomQualityIndex)
	{
		return;
	}

	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetOverallScalabilityLevel(Index);
		ApplyAndSaveGraphics();

		// 프리셋 하나가 세부 여섯 칸을 전부 덮어쓴다 — 콤보를 다시 읽어 맞춘다.
		RefreshFromSettings();
	}
}

void UBGSystemMenuWidget::ApplyDetailQuality(int32 RowIndex)
{
	if (bRefreshing || !DetailCombos.IsValidIndex(RowIndex) || !DetailCombos[RowIndex])
	{
		return;
	}

	const int32 Index = DetailCombos[RowIndex]->GetSelectedIndex();
	if (Index < 0 || Index >= CustomQualityIndex)
	{
		return;
	}

	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		(Settings->*DetailSetters[RowIndex])(Index);
		ApplyAndSaveGraphics();

		// 세부를 건드리면 전체 품질은 프리셋에서 벗어난다 — "사용자 지정" 으로 내려앉는다.
		RefreshFromSettings();
	}
}

void UBGSystemMenuWidget::HandleShadowChanged(FString SelectedItem, ESelectInfo::Type SelectionType)             { ApplyDetailQuality(0); }
void UBGSystemMenuWidget::HandleTextureChanged(FString SelectedItem, ESelectInfo::Type SelectionType)            { ApplyDetailQuality(1); }
void UBGSystemMenuWidget::HandleEffectsChanged(FString SelectedItem, ESelectInfo::Type SelectionType)            { ApplyDetailQuality(2); }
void UBGSystemMenuWidget::HandleAntiAliasingChanged(FString SelectedItem, ESelectInfo::Type SelectionType)       { ApplyDetailQuality(3); }
void UBGSystemMenuWidget::HandlePostProcessChanged(FString SelectedItem, ESelectInfo::Type SelectionType)        { ApplyDetailQuality(4); }
void UBGSystemMenuWidget::HandleGlobalIlluminationChanged(FString SelectedItem, ESelectInfo::Type SelectionType) { ApplyDetailQuality(5); }

void UBGSystemMenuWidget::HandleFrameLimitChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRefreshing || !FrameLimitCombo)
	{
		return;
	}

	const int32 Index = FrameLimitCombo->GetSelectedIndex();
	if (!FrameLimitValues.IsValidIndex(Index))
	{
		return;
	}

	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetFrameRateLimit(FrameLimitValues[Index]);
		ApplyAndSaveGraphics();
	}
}

void UBGSystemMenuWidget::HandleVSyncChanged(bool bIsChecked)
{
	if (bRefreshing)
	{
		return;
	}

	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetVSyncEnabled(bIsChecked);
		ApplyAndSaveGraphics();
	}
}

void UBGSystemMenuWidget::HandleMasterVolumeChanged(float Value)
{
	if (bRefreshing)
	{
		return;
	}

	MasterVolume = FMath::Clamp(Value, 0.f, 1.f);
	RefreshVolumeTexts();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->SetMasterVolume(MasterVolume);
	}
}

void UBGSystemMenuWidget::HandleMusicVolumeChanged(float Value)
{
	if (bRefreshing)
	{
		return;
	}

	MusicVolume = FMath::Clamp(Value, 0.f, 1.f);
	RefreshVolumeTexts();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->SetMusicVolume(MusicVolume);
	}
}

void UBGSystemMenuWidget::HandleSFXVolumeChanged(float Value)
{
	if (bRefreshing)
	{
		return;
	}

	SFXVolume = FMath::Clamp(Value, 0.f, 1.f);
	RefreshVolumeTexts();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->SetSFXVolume(SFXVolume);
	}
}

// ======================================================================================
//  도움말
// ======================================================================================

bool UBGSystemMenuWidget::IsSettingsOpen() const
{
	return SettingsPanel && SettingsPanel->GetVisibility() != ESlateVisibility::Collapsed;
}

bool UBGSystemMenuWidget::IsHelpOpen() const
{
	return HelpPanel && HelpPanel->GetVisibility() != ESlateVisibility::Collapsed;
}

void UBGSystemMenuWidget::BuildHelpRows()
{
	if (!RowClass)
	{
		return;
	}

	if (Grid_Jobs)
	{
		//	NativeConstruct 는 위젯이 다시 뷰포트에 붙을 때마다 불릴 수 있다 — 행이 쌓이지 않게 비운다.
		Grid_Jobs->ClearChildren();

		int32 Count = 0;
		if (CharacterTable)
		{
			for (const TPair<FName, uint8*>& Pair : CharacterTable->GetRowMap())
			{
				if (const FBGCharacterRow* Row = reinterpret_cast<const FBGCharacterRow*>(Pair.Value))
				{
					AddHelpRow(*Grid_Jobs, Count++, HelpJobTitle(*Row), Row->Description, FindHelpIcon(JobIcons, Row->CharacterTag));
				}
			}
		}
		PadFirstHelpRow(*Grid_Jobs, Count);
	}

	if (Grid_Roles)
	{
		Grid_Roles->ClearChildren();

		int32 Count = 0;
		for (int32 Index = 0; Index < HelpRoleCount; ++Index)
		{
			const FGameplayTag& Tag = HelpRoleOrder()[Index];

			//	Row 이름은 태그 문자열 그대로다. 없으면 그 역할만 빠진다 — 경고는 끈다.
			const FBGInfoTooltipRow* Row = InfoTable
				? InfoTable->FindRow<FBGInfoTooltipRow>(FName(*Tag.ToString()), TEXT("BGHelp Role"), /*bWarnIfMissing=*/ false)
				: nullptr;

			if (Row)
			{
				AddHelpRow(*Grid_Roles, Count++, Row->Title, Row->Body, FindHelpIcon(RoleIcons, Tag));
			}
		}
		PadFirstHelpRow(*Grid_Roles, Count);
	}

	if (Grid_Equips)
	{
		Grid_Equips->ClearChildren();

		int32 Count = 0;
		if (CardTable)
		{
			for (const TPair<FName, uint8*>& Pair : CardTable->GetRowMap())
			{
				const FBGCardRow* Row = reinterpret_cast<const FBGCardRow*>(Pair.Value);
				if (Row && Row->TypeTag == BG::Card::Type::Blue.GetTag())
				{
					AddHelpRow(*Grid_Equips, Count++, Row->DisplayName, Row->Description, FindHelpIcon(EquipIcons, Row->CardIdTag));
				}
			}
		}
		PadFirstHelpRow(*Grid_Equips, Count);
	}
}

void UBGSystemMenuWidget::AddHelpRow(UUniformGridPanel& Grid, int32 Index, const FText& Title, const FText& Body, UTexture2D* Icon)
{
	UBGHelpRowWidget* Row = CreateWidget<UBGHelpRowWidget>(this, RowClass);
	if (!Row)
	{
		return;
	}

	Row->SetInfo(Title, Body);
	Row->SetIcon(Icon);

	//	행 사이 세로 여백은 WBP_BGHelpRow 가 자기 패딩으로, 열 사이 여백은 격자의 SlotPadding 이 갖는다.
	const int32 ColumnCount = FMath::Max(1, Columns);
	Grid.AddChildToUniformGrid(Row, Index / ColumnCount, Index % ColumnCount);
}

void UBGSystemMenuWidget::PadFirstHelpRow(UUniformGridPanel& Grid, int32 Count)
{
	for (int32 Index = Count; Index < Columns; ++Index)
	{
		Grid.AddChildToUniformGrid(NewObject<USpacer>(this), 0, Index);
	}
}

void UBGSystemMenuWidget::SelectHelpTab(int32 Index)
{
	if (Scroll_Body)
	{
		Scroll_Body->ScrollToStart();
	}

	UButton* Buttons[] = { Tab_Jobs, Tab_Roles, Tab_Equips };
	UTextBlock* Texts[] = { Tab_Jobs_Text, Tab_Roles_Text, Tab_Equips_Text };
	UUniformGridPanel* Pages[] = { Grid_Jobs, Grid_Roles, Grid_Equips };
	for (int32 Tab = 0; Tab < UE_ARRAY_COUNT(Buttons); ++Tab)
	{
		const bool bActive = (Tab == Index);
		if (Pages[Tab])
		{
			//	Collapsed 가 아니라 Hidden — 자리를 남겨야 Overlay 가 가장 큰 탭 크기를 유지한다.
			Pages[Tab]->SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
		}
		if (Buttons[Tab])
		{
			Buttons[Tab]->SetBackgroundColor(bActive ? FLinearColor::White : InactiveTabTint);
		}
		if (Texts[Tab])
		{
			Texts[Tab]->SetColorAndOpacity(bActive ? ActiveTabColor : InactiveTabColor);
		}
	}
}

void UBGSystemMenuWidget::OpenHelp()
{
	// 패널은 한 번에 하나만 띄운다.
	CloseSettings();

	if (HelpPanel)
	{
		HelpPanel->SetVisibility(ESlateVisibility::Visible);
	}
	PopupAnim.Start(Help_Size_Popup);
}

void UBGSystemMenuWidget::CloseHelp()
{
	if (HelpPanel)
	{
		HelpPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBGSystemMenuWidget::HandleHelpClicked()
{
	if (IsHelpOpen())
	{
		CloseHelp();
	}
	else
	{
		OpenHelp();
	}
}

void UBGSystemMenuWidget::HandleHelpCloseClicked()
{
	CloseHelp();
}

void UBGSystemMenuWidget::HandleJobsTabClicked()
{
	SelectHelpTab(0);
}

void UBGSystemMenuWidget::HandleRolesTabClicked()
{
	SelectHelpTab(1);
}

void UBGSystemMenuWidget::HandleEquipsTabClicked()
{
	SelectHelpTab(2);
}

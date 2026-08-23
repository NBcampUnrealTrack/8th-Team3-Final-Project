// TRSettingsWidget.cpp

#include "UI/Settings/TRSettingsWidget.h"

#include "CommonInputModeTypes.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Settings/TRSettingsSubsystem.h"
#include "UI/Settings/TRSettingComboWidget.h"
#include "UI/Settings/TRSettingInfoWidget.h"
#include "UI/Settings/TRSettingsCommonButton.h"

// 설정 UI에서 공통으로 사용하는 선택지와 표시값 변환 함수
namespace TRSettingsUI
{
	const TArray<FString> QualityOptions = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
	const TArray<FString> OverallQualityOptions = { TEXT("Custom"), TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
	const TArray<FString> ToggleOptions = { TEXT("Off"), TEXT("On") };
	const TArray<FString> DisplayModeOptions = { TEXT("Fullscreen"), TEXT("Borderless"), TEXT("Windowed") };
	const TArray<FString> FrameRateOptions = { TEXT("Unlimited"), TEXT("30"), TEXT("60"), TEXT("120"), TEXT("144"), TEXT("165"), TEXT("240") };

	FString QualityToString(const ETRQualityLevel Level)
	{
		return QualityOptions[static_cast<int32>(Level)];
	}

	FString OverallQualityToString(const int32 Level)
	{
		return QualityOptions.IsValidIndex(Level) ? QualityOptions[Level] : TEXT("Custom");
	}

	TOptional<ETRQualityLevel> StringToQuality(const FString& Value)
	{
		const int32 Index = QualityOptions.IndexOfByKey(Value);
		return QualityOptions.IsValidIndex(Index)
			? TOptional<ETRQualityLevel>(static_cast<ETRQualityLevel>(Index))
			: TOptional<ETRQualityLevel>();
	}

	TOptional<ETRGraphicsQuality> GetGraphicsQuality(const ETRSettingId SettingId)
	{
		switch (SettingId)
		{
		case ETRSettingId::ViewDistanceQuality:
			return ETRGraphicsQuality::ViewDistance;
		case ETRSettingId::AntiAliasingQuality:
			return ETRGraphicsQuality::AntiAliasing;
		case ETRSettingId::ShadowQuality:
			return ETRGraphicsQuality::Shadow;
		case ETRSettingId::GlobalIlluminationQuality:
			return ETRGraphicsQuality::GlobalIllumination;
		case ETRSettingId::ReflectionQuality:
			return ETRGraphicsQuality::Reflection;
		case ETRSettingId::PostProcessQuality:
			return ETRGraphicsQuality::PostProcess;
		case ETRSettingId::TextureQuality:
			return ETRGraphicsQuality::Texture;
		case ETRSettingId::EffectsQuality:
			return ETRGraphicsQuality::Effects;
		case ETRSettingId::FoliageQuality:
			return ETRGraphicsQuality::Foliage;
		case ETRSettingId::ShadingQuality:
			return ETRGraphicsQuality::Shading;
		case ETRSettingId::LandscapeQuality:
			return ETRGraphicsQuality::Landscape;
		default:
			return {};
		}
	}

	FString ToggleToString(const bool bEnabled)
	{
		return bEnabled ? TEXT("On") : TEXT("Off");
	}

	FString ResolutionToString(const FIntPoint Value)
	{
		return FString::Printf(TEXT("%d x %d"), Value.X, Value.Y);
	}

	FIntPoint StringToResolution(const FString& Value)
	{
		FString Left, Right;
		return Value.Split(TEXT("x"), &Left, &Right)
			? FIntPoint(FCString::Atoi(*Left.TrimStartAndEnd()), FCString::Atoi(*Right.TrimStartAndEnd()))
			: FIntPoint::ZeroValue;
	}
	FString FrameRateToString(const float Value)
	{
		return Value <= 0.0f ? TEXT("Unlimited") : FString::FromInt(FMath::RoundToInt(Value));
	}
}

// CommonUI 메뉴 입력, Back 처리, 포커스 복원을 기본 활성화
UTRSettingsWidget::UTRSettingsWidget()
{
	bAutoActivate = true;
	bIsBackHandler = true;
	bAutoRestoreFocus = true;
}

// WBP 버튼/설정 행 이벤트 바인딩 후 저장된 설정값 표시
void UTRSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GraphicsTabButton->OnClicked().RemoveAll(this);
	GraphicsTabButton->OnClicked().AddUObject(this, &UTRSettingsWidget::HandleGraphicsTabClicked);
	AudioTabButton->OnClicked().RemoveAll(this);
	AudioTabButton->OnClicked().AddUObject(this, &UTRSettingsWidget::HandleAudioTabClicked);
	ControlTabButton->OnClicked().RemoveAll(this);
	ControlTabButton->OnClicked().AddUObject(this, &UTRSettingsWidget::HandleControlTabClicked);
	DefaultsButton->OnClicked().RemoveAll(this);
	DefaultsButton->OnClicked().AddUObject(this, &UTRSettingsWidget::HandleDefaultsClicked);
	CancelButton->OnClicked().RemoveAll(this);
	CancelButton->OnClicked().AddUObject(this, &UTRSettingsWidget::HandleCancelClicked);
	ApplyButton->OnClicked().RemoveAll(this);
	ApplyButton->OnClicked().AddUObject(this, &UTRSettingsWidget::HandleApplyClicked);
	AutoDetectButton->OnClicked().RemoveAll(this);
	AutoDetectButton->OnClicked().AddUObject(this, &UTRSettingsWidget::HandleAutoDetectClicked);
	BackButton->OnClicked().RemoveAll(this);
	BackButton->OnClicked().AddUObject(this, &UTRSettingsWidget::HandleBackClicked);
	for (UTRSettingsCommonButton* Tab : {GraphicsTabButton.Get(), AudioTabButton.Get(), ControlTabButton.Get()})
	{
		Tab->SetIsSelectable(true);
		Tab->SetIsInteractableWhenSelected(true);
	}
	InitializeRows();
	RefreshFromSettings();
	SetGraphicsTab();
}

// 화면 재진입 시 외부에서 변경된 설정값과 포커스 갱신
void UTRSettingsWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshFromSettings();
	RequestRefreshFocus();
}

// CommonUI Back 액션을 취소 동작과 동일하게 처리
bool UTRSettingsWidget::NativeOnHandleBackAction()
{
	HandleBackClicked();
	return true;
}

// 키보드/게임패드 입력 시작 포커스를 Graphics 탭으로 지정
UWidget* UTRSettingsWidget::NativeGetDesiredFocusTarget() const
{
	return GraphicsTabButton;
}

// 설정 화면 활성화 중 게임 입력 대신 CommonUI Menu 입력 사용
TOptional<FUIInputConfig> UTRSettingsWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock, false);
}

// 지원 해상도/모니터와 각 설정 범위를 행 위젯에 등록
void UTRSettingsWidget::InitializeRows()
{
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	if (!S)
	{
		return;
	}

	TArray<FString> ResolutionOptions;
	for (const FIntPoint Resolution : S->GetAvailableResolutions())
	{
		ResolutionOptions.Add(TRSettingsUI::ResolutionToString(Resolution));
	}

	DisplayModeSetting->InitializeSetting(ETRSettingId::DisplayMode, FText::FromString(TEXT("Display Mode")), TRSettingsUI::DisplayModeOptions, TEXT("Borderless"));
	DisplayMonitorSetting->InitializeSetting(ETRSettingId::DisplayMonitor, FText::FromString(TEXT("Display Monitor")), S->GetAvailableMonitorOptions(), S->GetCurrentMonitorOption());
	ResolutionSetting->InitializeSetting(ETRSettingId::Resolution, FText::FromString(TEXT("Display Resolution")), ResolutionOptions, TRSettingsUI::ResolutionToString(S->GetScreenResolution()));
	HDRSetting->InitializeSetting(ETRSettingId::HDR, FText::FromString(S->SupportsHDRDisplayOutput() ? TEXT("HDR Output") : TEXT("HDR Output (Unsupported)")), TRSettingsUI::ToggleOptions, TEXT("Off"));
	HDRDisplayNitsSetting->InitializeSetting(ETRSettingId::HDRDisplayNits, FText::FromString(TEXT("HDR Peak Brightness (nits)")), 400.0f, 4000.0f, 50.0f, S->GetHDRDisplayNits());
	HDRPaperWhiteSetting->InitializeSetting(ETRSettingId::HDRPaperWhite, FText::FromString(TEXT("HDR Paper White (nits)")), 80.0f, 500.0f, 1.0f, S->GetHDRPaperWhiteNits());
	HDRUILuminanceSetting->InitializeSetting(ETRSettingId::HDRUILuminance, FText::FromString(TEXT("HDR UI Brightness (nits)")), 80.0f, 500.0f, 1.0f, S->GetHDRUILuminanceNits());
	HDRUISeparateSetting->InitializeSetting(ETRSettingId::HDRUISeparate, FText::FromString(TEXT("Separate HDR UI Brightness")), TRSettingsUI::ToggleOptions, TEXT("Off"));
	HDRCalibrationSetting->InitializeSetting(ETRSettingId::HDRCalibration, FText::FromString(TEXT("Use HDR Calibration")), TRSettingsUI::ToggleOptions, TEXT("Off"));
	UWidget* const HDRRows[] = {HDRSetting.Get(), HDRDisplayNitsSetting.Get(), HDRPaperWhiteSetting.Get(), HDRUILuminanceSetting.Get(), HDRUISeparateSetting.Get(), HDRCalibrationSetting.Get()};
	for (UWidget* HDRRow : HDRRows)
	{
		HDRRow->SetIsEnabled(S->SupportsHDRDisplayOutput());
	}
	OverallQualitySetting->InitializeSetting(ETRSettingId::OverallQuality, FText::FromString(TEXT("Overall Graphics")), TRSettingsUI::OverallQualityOptions, TEXT("Custom"));
	ResolutionScaleSetting->InitializeSetting(ETRSettingId::ResolutionScale, FText::FromString(TEXT("Resolution Scale")), 25.0f, 100.0f, 1.0f, S->GetResolutionScale());
	VSyncSetting->InitializeSetting(ETRSettingId::VSync, FText::FromString(TEXT("V-Sync")), TRSettingsUI::ToggleOptions, TEXT("Off"));
	DynamicResolutionSetting->InitializeSetting(ETRSettingId::DynamicResolution, FText::FromString(TEXT("Dynamic Resolution")), TRSettingsUI::ToggleOptions, TEXT("Off"));
	FrameRateSetting->InitializeSetting(ETRSettingId::FrameRate, FText::FromString(TEXT("Frame Limit")), TRSettingsUI::FrameRateOptions, TEXT("Unlimited"));

	struct FQualityRow
	{
		UTRSettingComboWidget* Row;
		ETRSettingId SettingId;
		const TCHAR* Label;
	};
	const FQualityRow QualityRows[] = {
		{ViewDistanceQualitySetting, ETRSettingId::ViewDistanceQuality, TEXT("View Distance")}, {AntiAliasingQualitySetting, ETRSettingId::AntiAliasingQuality, TEXT("Anti-Aliasing")},
		{ShadowQualitySetting, ETRSettingId::ShadowQuality, TEXT("Shadow")}, {GlobalIlluminationQualitySetting, ETRSettingId::GlobalIlluminationQuality, TEXT("Global Illumination")},
		{ReflectionQualitySetting, ETRSettingId::ReflectionQuality, TEXT("Reflection")}, {PostProcessQualitySetting, ETRSettingId::PostProcessQuality, TEXT("Post Process")},
		{TextureQualitySetting, ETRSettingId::TextureQuality, TEXT("Texture")}, {EffectsQualitySetting, ETRSettingId::EffectsQuality, TEXT("Effects")},
		{FoliageQualitySetting, ETRSettingId::FoliageQuality, TEXT("Foliage")}, {ShadingQualitySetting, ETRSettingId::ShadingQuality, TEXT("Shading")},
		{LandscapeQualitySetting, ETRSettingId::LandscapeQuality, TEXT("Landscape")}
	};
	for (const FQualityRow& Item : QualityRows)
	{
		Item.Row->InitializeSetting(Item.SettingId, FText::FromString(Item.Label), TRSettingsUI::QualityOptions, TEXT("Epic"));
		Item.Row->OnSettingOptionChanged.AddUniqueDynamic(this, &UTRSettingsWidget::HandleOptionChanged);
	}

	MasterVolumeSetting->InitializeSetting(ETRSettingId::MasterVolume, FText::FromString(TEXT("Master Volume")), 0.0f, 1.0f, 0.01f, S->GetMasterVolume());
	MusicVolumeSetting->InitializeSetting(ETRSettingId::MusicVolume, FText::FromString(TEXT("Background Music")), 0.0f, 1.0f, 0.01f, S->GetMusicVolume());
	EffectsVolumeSetting->InitializeSetting(ETRSettingId::EffectsVolume, FText::FromString(TEXT("Sound Effects")), 0.0f, 1.0f, 0.01f, S->GetEffectsVolume());
	SensitivitySetting->InitializeSetting(ETRSettingId::LookSensitivity, FText::FromString(TEXT("Look Sensitivity")), 0.1f, 5.0f, 0.05f, S->GetLookSensitivity());

	for (UTRSettingInfoWidget* Row : {ResolutionScaleSetting.Get(), HDRDisplayNitsSetting.Get(), HDRPaperWhiteSetting.Get(), HDRUILuminanceSetting.Get(), MasterVolumeSetting.Get(), MusicVolumeSetting.Get(), EffectsVolumeSetting.Get(), SensitivitySetting.Get()})
	{
		Row->OnSettingValueChanged.AddUniqueDynamic(this, &UTRSettingsWidget::HandleSliderValueChanged);
	}
	for (UTRSettingComboWidget* Row : {DisplayModeSetting.Get(), DisplayMonitorSetting.Get(), ResolutionSetting.Get(), HDRSetting.Get(), HDRUISeparateSetting.Get(), HDRCalibrationSetting.Get(), OverallQualitySetting.Get(), VSyncSetting.Get(), DynamicResolutionSetting.Get(), FrameRateSetting.Get()})
	{
		Row->OnSettingOptionChanged.AddUniqueDynamic(this, &UTRSettingsWidget::HandleOptionChanged);
	}
}

// UTRSettingsSubsystem의 현재값을 이벤트 재호출 없이 모든 행에 반영
void UTRSettingsWidget::RefreshFromSettings()
{
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	if (!S)
	{
		SetStatus(TEXT("Settings subsystem unavailable"), false);
		return;
	}
	const int32 Mode = FMath::Clamp(S->GetWindowMode(), 0, 2);
	DisplayModeSetting->SetSelectedOption(TRSettingsUI::DisplayModeOptions[Mode]);
	DisplayMonitorSetting->SetSelectedOption(S->GetCurrentMonitorOption());
	ResolutionSetting->SetSelectedOption(TRSettingsUI::ResolutionToString(S->GetScreenResolution()));
	HDRSetting->SetSelectedOption(TRSettingsUI::ToggleToString(S->IsHDREnabled()));
	HDRDisplayNitsSetting->SetValue(S->GetHDRDisplayNits());
	HDRPaperWhiteSetting->SetValue(S->GetHDRPaperWhiteNits());
	HDRUILuminanceSetting->SetValue(S->GetHDRUILuminanceNits());
	HDRUISeparateSetting->SetSelectedOption(TRSettingsUI::ToggleToString(S->IsHDRUILuminanceSeparate()));
	HDRCalibrationSetting->SetSelectedOption(TRSettingsUI::ToggleToString(S->IsHDRCalibrationUsed()));
	OverallQualitySetting->SetSelectedOption(TRSettingsUI::OverallQualityToString(S->GetOverallQuality()));
	ResolutionScaleSetting->SetValue(S->GetResolutionScale());
	VSyncSetting->SetSelectedOption(TRSettingsUI::ToggleToString(S->IsVSyncEnabled()));
	DynamicResolutionSetting->SetSelectedOption(TRSettingsUI::ToggleToString(S->IsDynamicResolutionEnabled()));
	FrameRateSetting->SetSelectedOption(TRSettingsUI::FrameRateToString(S->GetFrameRateLimit()));

	struct FQualityRow
	{
		UTRSettingComboWidget* Row;
		ETRGraphicsQuality Quality;
	};
	const FQualityRow Rows[] = {{ViewDistanceQualitySetting, ETRGraphicsQuality::ViewDistance}, {AntiAliasingQualitySetting, ETRGraphicsQuality::AntiAliasing},
		{ShadowQualitySetting, ETRGraphicsQuality::Shadow}, {GlobalIlluminationQualitySetting, ETRGraphicsQuality::GlobalIllumination}, {ReflectionQualitySetting, ETRGraphicsQuality::Reflection},
		{PostProcessQualitySetting, ETRGraphicsQuality::PostProcess}, {TextureQualitySetting, ETRGraphicsQuality::Texture}, {EffectsQualitySetting, ETRGraphicsQuality::Effects},
		{FoliageQualitySetting, ETRGraphicsQuality::Foliage}, {ShadingQualitySetting, ETRGraphicsQuality::Shading}, {LandscapeQualitySetting, ETRGraphicsQuality::Landscape}};
	for (const FQualityRow& Item : Rows)
	{
		Item.Row->SetSelectedOption(TRSettingsUI::QualityToString(S->GetQualityLevel(Item.Quality)));
	}
	MasterVolumeSetting->SetValue(S->GetMasterVolume());
	MusicVolumeSetting->SetValue(S->GetMusicVolume());
	EffectsVolumeSetting->SetValue(S->GetEffectsVolume());
	SensitivitySetting->SetValue(S->GetLookSensitivity());
	SetStatus(TEXT("Loaded from GameUserSettings.ini"), true);
}

// 현재 GameInstance에 연결된 설정 Subsystem 반환
UTRSettingsSubsystem* UTRSettingsWidget::GetSettingsSubsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTRSettingsSubsystem>() : nullptr;
}

// 수치형 UI 입력을 해당 설정의 미저장 Preview 값으로 반영
void UTRSettingsWidget::HandleSliderValueChanged(const ETRSettingId SettingId, const float NewValue)
{
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	if (!S)
	{
		return;
	}

	switch (SettingId)
	{
	case ETRSettingId::ResolutionScale:
		S->SetResolutionScale(NewValue);
		break;
	case ETRSettingId::HDRDisplayNits:
		S->SetHDRDisplayNits(FMath::RoundToInt(NewValue));
		break;
	case ETRSettingId::HDRPaperWhite:
		S->SetHDRPaperWhiteNits(NewValue);
		break;
	case ETRSettingId::HDRUILuminance:
		S->SetHDRUILuminanceNits(NewValue);
		break;
	case ETRSettingId::MasterVolume:
		S->SetMasterVolume(NewValue);
		break;
	case ETRSettingId::MusicVolume:
		S->SetMusicVolume(NewValue);
		break;
	case ETRSettingId::EffectsVolume:
		S->SetEffectsVolume(NewValue);
		break;
	case ETRSettingId::LookSensitivity:
		S->SetLookSensitivity(NewValue);
		break;
	default:
		ensureMsgf(false, TEXT("TRSettings: unsupported slider setting enum value"));
		return;
	}
	SetStatus(TEXT("Previewing unsaved changes"), true);
}

// 선택형 UI 입력을 해당 설정의 미저장 Preview 값으로 반영
void UTRSettingsWidget::HandleOptionChanged(const ETRSettingId SettingId, const FString& SelectedOption)
{
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	if (!S)
	{
		return;
	}

	switch (SettingId)
	{
	case ETRSettingId::DisplayMode:
	{
		const int32 Mode = TRSettingsUI::DisplayModeOptions.IndexOfByKey(SelectedOption);
		if (Mode == INDEX_NONE)
		{
			return;
		}
		S->SetWindowMode(Mode);
		break;
	}
	case ETRSettingId::DisplayMonitor:
		S->SetMonitorOption(SelectedOption);
		break;
	case ETRSettingId::Resolution:
	{
		const FIntPoint Resolution = TRSettingsUI::StringToResolution(SelectedOption);
		if (Resolution.X <= 0 || Resolution.Y <= 0)
		{
			return;
		}
		S->SetScreenResolution(Resolution);
		break;
	}
	case ETRSettingId::HDR:
		S->SetHDREnabled(SelectedOption == TEXT("On"));
		break;
	case ETRSettingId::HDRUISeparate:
		S->SetHDRUILuminanceSeparate(SelectedOption == TEXT("On"));
		break;
	case ETRSettingId::HDRCalibration:
		S->SetHDRCalibrationUsed(SelectedOption == TEXT("On"));
		break;
	case ETRSettingId::OverallQuality:
	{
		const TOptional<ETRQualityLevel> Level = TRSettingsUI::StringToQuality(SelectedOption);
		if (Level.IsSet())
		{
			S->SetOverallQuality(static_cast<int32>(Level.GetValue()));
			RefreshFromSettings();
		}
		break;
	}
	case ETRSettingId::VSync:
		S->SetVSyncEnabled(SelectedOption == TEXT("On"));
		break;
	case ETRSettingId::DynamicResolution:
		S->SetDynamicResolutionEnabled(SelectedOption == TEXT("On"));
		break;
	case ETRSettingId::FrameRate:
		S->SetFrameRateLimit(SelectedOption == TEXT("Unlimited") ? 0.0f : FCString::Atof(*SelectedOption));
		break;
	default:
	{
		const TOptional<ETRGraphicsQuality> Quality = TRSettingsUI::GetGraphicsQuality(SettingId);
		const TOptional<ETRQualityLevel> Level = TRSettingsUI::StringToQuality(SelectedOption);
		if (!Quality.IsSet() || !Level.IsSet())
		{
			ensureMsgf(false, TEXT("TRSettings: unsupported combo setting enum value"));
			return;
		}
		S->SetQualityLevel(Quality.GetValue(), Level.GetValue());
		OverallQualitySetting->SetSelectedOption(TEXT("Custom"));
		break;
	}
	}
	SetStatus(TEXT("Previewing unsaved changes"), true);
}

// Graphics/Audio/Control 페이지와 탭 선택 색상 동기화
void UTRSettingsWidget::HandleGraphicsTabClicked()
{
	SetGraphicsTab();
}

void UTRSettingsWidget::HandleAudioTabClicked()
{
	SettingsSwitcher->SetActiveWidgetIndex(1);
	GraphicsTabButton->SetSettingsSelected(false);
	AudioTabButton->SetSettingsSelected(true);
	ControlTabButton->SetSettingsSelected(false);
}

void UTRSettingsWidget::HandleControlTabClicked()
{
	SettingsSwitcher->SetActiveWidgetIndex(2);
	GraphicsTabButton->SetSettingsSelected(false);
	AudioTabButton->SetSettingsSelected(false);
	ControlTabButton->SetSettingsSelected(true);
}

void UTRSettingsWidget::SetGraphicsTab()
{
	SettingsSwitcher->SetActiveWidgetIndex(0);
	GraphicsTabButton->SetSettingsSelected(true);
	AudioTabButton->SetSettingsSelected(false);
	ControlTabButton->SetSettingsSelected(false);
}

// Preview 설정을 게임에 적용하고 GameUserSettings.ini에 저장
void UTRSettingsWidget::HandleApplyClicked()
{
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	const bool bOK = S && S->ApplySettings(true);
	SetStatus(bOK ? TEXT("Applied and saved") : TEXT("Apply failed"), bOK);
}

// 저장하지 않은 변경을 폐기하고 마지막 저장값 복원
void UTRSettingsWidget::HandleCancelClicked()
{
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	const bool bOK = S && S->ReloadSettings();
	RefreshFromSettings();
	SetStatus(bOK ? TEXT("Unsaved changes discarded") : TEXT("Reload failed"), bOK);
}

// 기본값을 Preview 상태로 불러오며 Apply 전까지 저장하지 않음
void UTRSettingsWidget::HandleDefaultsClicked()
{
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	const bool bOK = S && S->ResetToDefaults();
	RefreshFromSettings();
	SetStatus(bOK ? TEXT("Defaults staged; Apply to save") : TEXT("Default reset failed"), bOK);
}

// 하드웨어 벤치마크 결과를 권장 그래픽 Preview 값으로 반영
void UTRSettingsWidget::HandleAutoDetectClicked()
{
	SetStatus(TEXT("Detecting recommended graphics settings..."), true);
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	const bool bOK = S && S->RunHardwareBenchmark(10);
	RefreshFromSettings();
	SetStatus(bOK ? TEXT("Recommended graphics staged; Apply to save") : TEXT("Hardware benchmark failed"), bOK);
}

// 저장하지 않은 변경을 폐기한 뒤 CommonUI 화면 비활성화
void UTRSettingsWidget::HandleBackClicked()
{
	if (UTRSettingsSubsystem* S = GetSettingsSubsystem())
	{
		S->ReloadSettings();
	}
	DeactivateWidget();
}

// 마지막 설정 동작 결과와 성공 여부를 화면 하단에 표시
void UTRSettingsWidget::SetStatus(const FString& Message, const bool bSuccess)
{
	StatusText->SetText(FText::FromString(Message));
	StatusText->SetColorAndOpacity(FSlateColor(bSuccess ? FLinearColor(0.45f, 0.85f, 0.70f) : FLinearColor(1.0f, 0.3f, 0.25f)));
}

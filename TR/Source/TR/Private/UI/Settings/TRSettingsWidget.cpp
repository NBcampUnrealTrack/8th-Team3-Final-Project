// TRSettingsWidget.cpp

#include "UI/Settings/TRSettingsWidget.h"

#include "CommonInputModeTypes.h"
#include "Components/TextBlock.h"
#include "CommonAnimatedSwitcher.h"
#include "UI/Settings/TRSettingsTabList.h"
#include "Engine/GameInstance.h"
#include "Settings/TRSettingsSubsystem.h"
#include "UI/Settings/TRSettingComboWidget.h"
#include "UI/Settings/TRSettingInfoWidget.h"
#include "UI/Settings/TRSettingsCommonButton.h"

// 설정 UI에서 공통으로 사용하는 선택지와 표시값 변환 함수
namespace TRSettingsUI
{
	// 콤보 선택지는 화면에 그대로 보이면서 비교값으로도 쓰인다. 글자를 고칠 때 한쪽만
	// 바뀌면 조용히 안 먹으므로, 비교에 쓰는 것은 상수로 묶어 둔다.
	const FString OptionOn = TEXT("켜기");
	const FString OptionOff = TEXT("끄기");
	const FString OptionUnlimited = TEXT("제한 없음");
	const FString OptionCustom = TEXT("사용자 지정");

	const TArray<FString> QualityOptions = { TEXT("낮음"), TEXT("보통"), TEXT("높음"), TEXT("매우 높음"), TEXT("시네마틱") };
	const TArray<FString> OverallQualityOptions = { OptionCustom, TEXT("낮음"), TEXT("보통"), TEXT("높음"), TEXT("매우 높음"), TEXT("시네마틱") };
	const TArray<FString> ToggleOptions = { OptionOff, OptionOn };
	const TArray<FString> DisplayModeOptions = { TEXT("전체 화면"), TEXT("테두리 없는 창"), TEXT("창 모드") };
	const TArray<FString> FrameRateOptions = { OptionUnlimited, TEXT("30"), TEXT("60"), TEXT("120"), TEXT("144"), TEXT("165"), TEXT("240") };

	FString QualityToString(const ETRQualityLevel Level)
	{
		return QualityOptions[static_cast<int32>(Level)];
	}

	FString OverallQualityToString(const int32 Level)
	{
		return QualityOptions.IsValidIndex(Level) ? QualityOptions[Level] : OptionCustom;
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
		return bEnabled ? OptionOn : OptionOff;
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
		return Value <= 0.0f ? OptionUnlimited : FString::FromInt(FMath::RoundToInt(Value));
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
	SetUpTabs();
	InitializeRows();
	RefreshFromSettings();

	// bAutoActivate 로 Super::NativeConstruct 안에서 이미 활성화됐다. 그때는 탭이
	// 없어 포커스가 BackButton 으로 떨어지므로, 탭이 생긴 지금 다시 잡는다.
	RequestRefreshFocus();
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

// 키보드/게임패드 입력 시작 포커스를 지금 선택된 탭으로 지정
UWidget* UTRSettingsWidget::NativeGetDesiredFocusTarget() const
{
	if (IsValid(TabList))
	{
		if (UCommonButtonBase* ActiveTab = TabList->GetTabButtonBaseByID(TabList->GetSelectedTabId()))
		{
			return ActiveTab;
		}
	}

	// 탭이 아직 없으면 뒤로 버튼이라도 잡아 준다. 포커스가 없으면 Back 액션이 안 온다.
	return BackButton;
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

	DisplayModeSetting->InitializeSetting(ETRSettingId::DisplayMode, FText::FromString(TEXT("화면 모드")), TRSettingsUI::DisplayModeOptions, TRSettingsUI::DisplayModeOptions[1]);
	DisplayMonitorSetting->InitializeSetting(ETRSettingId::DisplayMonitor, FText::FromString(TEXT("모니터")), S->GetAvailableMonitorOptions(), S->GetCurrentMonitorOption());
	ResolutionSetting->InitializeSetting(ETRSettingId::Resolution, FText::FromString(TEXT("해상도")), ResolutionOptions, TRSettingsUI::ResolutionToString(S->GetScreenResolution()));
	OverallQualitySetting->InitializeSetting(ETRSettingId::OverallQuality, FText::FromString(TEXT("전체 품질")), TRSettingsUI::OverallQualityOptions, TRSettingsUI::OptionCustom);
	ResolutionScaleSetting->InitializeSetting(ETRSettingId::ResolutionScale, FText::FromString(TEXT("해상도 배율")), 25.0f, 100.0f, 1.0f, S->GetResolutionScale());
	VSyncSetting->InitializeSetting(ETRSettingId::VSync, FText::FromString(TEXT("수직 동기화")), TRSettingsUI::ToggleOptions, TRSettingsUI::OptionOff);
	DynamicResolutionSetting->InitializeSetting(ETRSettingId::DynamicResolution, FText::FromString(TEXT("동적 해상도")), TRSettingsUI::ToggleOptions, TRSettingsUI::OptionOff);
	FrameRateSetting->InitializeSetting(ETRSettingId::FrameRate, FText::FromString(TEXT("프레임 제한")), TRSettingsUI::FrameRateOptions, TRSettingsUI::OptionUnlimited);

	struct FQualityRow
	{
		UTRSettingComboWidget* Row;
		ETRSettingId SettingId;
		const TCHAR* Label;
	};
	const FQualityRow QualityRows[] = {
		{ViewDistanceQualitySetting, ETRSettingId::ViewDistanceQuality, TEXT("시야 거리")}, {AntiAliasingQualitySetting, ETRSettingId::AntiAliasingQuality, TEXT("안티에일리어싱")},
		{ShadowQualitySetting, ETRSettingId::ShadowQuality, TEXT("그림자")}, {GlobalIlluminationQualitySetting, ETRSettingId::GlobalIlluminationQuality, TEXT("전역 조명")},
		{ReflectionQualitySetting, ETRSettingId::ReflectionQuality, TEXT("반사")}, {PostProcessQualitySetting, ETRSettingId::PostProcessQuality, TEXT("후처리")},
		{TextureQualitySetting, ETRSettingId::TextureQuality, TEXT("텍스처")}, {EffectsQualitySetting, ETRSettingId::EffectsQuality, TEXT("이펙트")},
		{FoliageQualitySetting, ETRSettingId::FoliageQuality, TEXT("식생")}, {ShadingQualitySetting, ETRSettingId::ShadingQuality, TEXT("셰이딩")},
		{LandscapeQualitySetting, ETRSettingId::LandscapeQuality, TEXT("랜드스케이프")}
	};
	for (const FQualityRow& Item : QualityRows)
	{
		Item.Row->InitializeSetting(Item.SettingId, FText::FromString(Item.Label), TRSettingsUI::QualityOptions, TRSettingsUI::QualityOptions[3]);
		Item.Row->OnSettingOptionChanged.AddUniqueDynamic(this, &UTRSettingsWidget::HandleOptionChanged);
	}

	MasterVolumeSetting->InitializeSetting(ETRSettingId::MasterVolume, FText::FromString(TEXT("전체 음량")), 0.0f, 1.0f, 0.01f, S->GetMasterVolume());
	MusicVolumeSetting->InitializeSetting(ETRSettingId::MusicVolume, FText::FromString(TEXT("배경 음악")), 0.0f, 1.0f, 0.01f, S->GetMusicVolume());
	EffectsVolumeSetting->InitializeSetting(ETRSettingId::EffectsVolume, FText::FromString(TEXT("효과음")), 0.0f, 1.0f, 0.01f, S->GetEffectsVolume());
	SensitivitySetting->InitializeSetting(ETRSettingId::LookSensitivity, FText::FromString(TEXT("마우스 감도")), 0.1f, 5.0f, 0.05f, S->GetLookSensitivity());

	for (UTRSettingInfoWidget* Row : {ResolutionScaleSetting.Get(), MasterVolumeSetting.Get(), MusicVolumeSetting.Get(), EffectsVolumeSetting.Get(), SensitivitySetting.Get()})
	{
		Row->OnSettingValueChanged.AddUniqueDynamic(this, &UTRSettingsWidget::HandleSliderValueChanged);
	}
	for (UTRSettingComboWidget* Row : {DisplayModeSetting.Get(), DisplayMonitorSetting.Get(), ResolutionSetting.Get(), OverallQualitySetting.Get(), VSyncSetting.Get(), DynamicResolutionSetting.Get(), FrameRateSetting.Get()})
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
		SetStatus(TEXT("설정 시스템을 찾을 수 없습니다"), false);
		return;
	}
	const int32 Mode = FMath::Clamp(S->GetWindowMode(), 0, 2);
	DisplayModeSetting->SetSelectedOption(TRSettingsUI::DisplayModeOptions[Mode]);
	DisplayMonitorSetting->SetSelectedOption(S->GetCurrentMonitorOption());
	ResolutionSetting->SetSelectedOption(TRSettingsUI::ResolutionToString(S->GetScreenResolution()));
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
	SetStatus(TEXT("저장된 설정을 불러왔습니다"), true);
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
	SetStatus(TEXT("저장하지 않은 변경을 미리 보는 중입니다"), true);
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
		S->SetVSyncEnabled(SelectedOption == TRSettingsUI::OptionOn);
		break;
	case ETRSettingId::DynamicResolution:
		S->SetDynamicResolutionEnabled(SelectedOption == TRSettingsUI::OptionOn);
		break;
	case ETRSettingId::FrameRate:
		S->SetFrameRateLimit(SelectedOption == TRSettingsUI::OptionUnlimited ? 0.0f : FCString::Atof(*SelectedOption));
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
		OverallQualitySetting->SetSelectedOption(TRSettingsUI::OptionCustom);
		break;
	}
	}
	SetStatus(TEXT("저장하지 않은 변경을 미리 보는 중입니다"), true);
}

// 등록만 해 두면 클릭, 전환, 배타 선택, 게임패드 이동은 CommonUI 가 알아서 한다.
void UTRSettingsWidget::SetUpTabs()
{
	if (!ensure(TabList) || !ensure(SettingsSwitcher))
	{
		return;
	}

	// 탭 버튼 WBP 를 안 골라 두면 탭이 하나도 안 생긴다. 조용히 비면 원인을 알 수 없다.
	if (!ensureMsgf(TabButtonClass, TEXT("WBP 의 TabButtonClass 가 비어 있다")))
	{
		return;
	}

	TabList->SetLinkedSwitcher(SettingsSwitcher);

	// 페이지는 이미 스위처의 자식이다. 등록은 탭과 페이지를 짝지어 주기만 한다.
	const TPair<FName, UWidget*> Pages[] =
	{
		{ TEXT("Graphics"), GraphicsPage },
		{ TEXT("Audio"),    AudioPage },
		{ TEXT("Control"),  ControlPage },
	};

	for (const TPair<FName, UWidget*>& Page : Pages)
	{
		if (IsValid(Page.Value))
		{
			TabList->RegisterTab(Page.Key, TabButtonClass, Page.Value);
		}
	}

	// 버튼은 등록 시점에 만들어지므로 글자는 그 뒤에 넣는다.
	ApplyTabLabel(TEXT("Graphics"), NSLOCTEXT("TR", "SettingsTabGraphics", "그래픽"));
	ApplyTabLabel(TEXT("Audio"), NSLOCTEXT("TR", "SettingsTabAudio", "사운드"));
	ApplyTabLabel(TEXT("Control"), NSLOCTEXT("TR", "SettingsTabControl", "조작"));

	TabList->SelectTabByID(TEXT("Graphics"), true);
}

void UTRSettingsWidget::ApplyTabLabel(const FName TabId, const FText& InLabel) const
{
	if (UTRSettingsCommonButton* TabButton = Cast<UTRSettingsCommonButton>(TabList->GetTabButtonBaseByID(TabId)))
	{
		TabButton->SetButtonText(InLabel);
	}
}

// Preview 설정을 게임에 적용하고 GameUserSettings.ini에 저장
void UTRSettingsWidget::HandleApplyClicked()
{
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	const bool bOK = S && S->ApplySettings(true);
	SetStatus(bOK ? TEXT("적용하고 저장했습니다") : TEXT("적용에 실패했습니다"), bOK);
}

// 저장하지 않은 변경을 폐기하고 마지막 저장값 복원
void UTRSettingsWidget::HandleCancelClicked()
{
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	const bool bOK = S && S->ReloadSettings();
	RefreshFromSettings();
	SetStatus(bOK ? TEXT("저장하지 않은 변경을 되돌렸습니다") : TEXT("되돌리기에 실패했습니다"), bOK);
}

// 기본값을 Preview 상태로 불러오며 Apply 전까지 저장하지 않음
void UTRSettingsWidget::HandleDefaultsClicked()
{
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	const bool bOK = S && S->ResetToDefaults();
	RefreshFromSettings();
	SetStatus(bOK ? TEXT("기본값을 적용했습니다. 저장하려면 적용하고 저장을 누르세요") : TEXT("기본값 복원에 실패했습니다"), bOK);
}

// 하드웨어 벤치마크 결과를 권장 그래픽 Preview 값으로 반영
void UTRSettingsWidget::HandleAutoDetectClicked()
{
	SetStatus(TEXT("권장 그래픽 설정을 측정하는 중입니다"), true);
	UTRSettingsSubsystem* S = GetSettingsSubsystem();
	const bool bOK = S && S->RunHardwareBenchmark(10);
	RefreshFromSettings();
	SetStatus(bOK ? TEXT("권장 설정을 적용했습니다. 저장하려면 적용하고 저장을 누르세요") : TEXT("성능 측정에 실패했습니다"), bOK);
}

// 저장하지 않은 변경을 폐기한 뒤 CommonUI 화면 비활성화
// 저장 안 한 변경을 되돌리고 닫는다. AUTO DETECT, DEFAULTS 는 누른 즉시 실행 상태를 바꾼다.
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

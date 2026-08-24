// TRSettingsSubsystem.cpp

#include "Settings/TRSettingsSubsystem.h"

#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/TRGameUserSettings.h"
#include "Settings/TRSettingsProjectSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectGlobals.h"

// 저장값 로드 후 그래픽 설정을 적용하고 맵 로드 시 오디오 적용 delegate 등록
void UTRSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTRSettingsSubsystem::HandlePostLoadMap);

	if (UTRGameUserSettings* Settings = GetUserSettings())
	{
		Settings->LoadSettings(false);
		Settings->ValidateSettings();
		PersistedScalabilityQuality = Settings->ScalabilityQuality;
		ApplyRuntimeSettingsWithoutSaving();
		ApplySoundSettings();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("TRSettings: GameUserSettingsClassName is not UTRGameUserSettings"));
	}
}

// 맵 로드 delegate와 한 번 Push한 SoundMix를 정리
void UTRSettingsSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (bSoundMixPushed && LoadedSoundMix && GetWorld())
	{
		UGameplayStatics::PopSoundMixModifier(GetWorld(), LoadedSoundMix);
	}
	bSoundMixPushed = false;
	Super::Deinitialize();
}

// GameInstanceSubsystem 초기화 후 생성된 게임 World에 저장 볼륨 적용
void UTRSettingsSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		// GameInstanceSubsystem 초기화 시점에 World가 없었던 경우 저장 볼륨을 여기서 적용
		ApplySoundSettings(LoadedWorld);
	}
}

// Engine에 등록된 GameUserSettings를 프로젝트 설정 클래스로 반환
UTRGameUserSettings* UTRSettingsSubsystem::GetUserSettings() const
{
	return GEngine ? Cast<UTRGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

// RHI 지원 해상도를 수집하고 현재 해상도와 최소 fallback 목록 보장
TArray<FIntPoint> UTRSettingsSubsystem::GetAvailableResolutions() const
{
	TArray<FIntPoint> Result;
	FScreenResolutionArray RHIResolutions;
	if (GDynamicRHI && RHIGetAvailableResolutions(RHIResolutions, true))
	{
		for (const FScreenResolutionRHI& Resolution : RHIResolutions)
		{
			const FIntPoint Size(Resolution.Width, Resolution.Height);
			if (Size.X >= 1280 && Size.Y >= 720)
			{
				Result.AddUnique(Size);
			}
		}
	}

	const FIntPoint Current = GetScreenResolution();
	if (Current.X > 0 && Current.Y > 0)
	{
		Result.AddUnique(Current);
	}
	if (Result.IsEmpty())
	{
		Result = { FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160) };
	}
	Result.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y; });
	return Result;
}

//--- 그래픽 설정 조회 ---
FIntPoint UTRSettingsSubsystem::GetScreenResolution() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetScreenResolution() : FIntPoint(1920, 1080);
}

int32 UTRSettingsSubsystem::GetWindowMode() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings ? static_cast<int32>(Settings->GetFullscreenMode()) : static_cast<int32>(EWindowMode::WindowedFullscreen);
}

bool UTRSettingsSubsystem::IsVSyncEnabled() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings && Settings->IsVSyncEnabled();
}

bool UTRSettingsSubsystem::IsDynamicResolutionEnabled() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings && Settings->IsDynamicResolutionEnabled();
}

float UTRSettingsSubsystem::GetFrameRateLimit() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetFrameRateLimit() : 0.0f;
}

// 엔진의 정규화 값 대신 UI에 표시할 실제 백분율 반환
float UTRSettingsSubsystem::GetResolutionScale() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	if (!Settings)
	{
		return 100.0f;
	}
	float Normalized = 1.0f, Value = 100.0f, Min = 25.0f, Max = 100.0f;
	Settings->GetResolutionScaleInformationEx(Normalized, Value, Min, Max);
	return Value;
}

int32 UTRSettingsSubsystem::GetOverallQuality() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetOverallScalabilityLevel() : 3;
}

// 열거형으로 지정한 개별 Scalability 품질 단계 반환
ETRQualityLevel UTRSettingsSubsystem::GetQualityLevel(const ETRGraphicsQuality Quality) const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	if (!Settings)
	{
		return ETRQualityLevel::Epic;
	}

	const Scalability::FQualityLevels& Levels = Settings->ScalabilityQuality;
	int32 Level = static_cast<int32>(ETRQualityLevel::Epic);
	switch (Quality)
	{
	case ETRGraphicsQuality::ViewDistance:
		Level = Levels.ViewDistanceQuality;
		break;
	case ETRGraphicsQuality::AntiAliasing:
		Level = Levels.AntiAliasingQuality;
		break;
	case ETRGraphicsQuality::Shadow:
		Level = Levels.ShadowQuality;
		break;
	case ETRGraphicsQuality::GlobalIllumination:
		Level = Levels.GlobalIlluminationQuality;
		break;
	case ETRGraphicsQuality::Reflection:
		Level = Levels.ReflectionQuality;
		break;
	case ETRGraphicsQuality::PostProcess:
		Level = Levels.PostProcessQuality;
		break;
	case ETRGraphicsQuality::Texture:
		Level = Levels.TextureQuality;
		break;
	case ETRGraphicsQuality::Effects:
		Level = Levels.EffectsQuality;
		break;
	case ETRGraphicsQuality::Foliage:
		Level = Levels.FoliageQuality;
		break;
	case ETRGraphicsQuality::Shading:
		Level = Levels.ShadingQuality;
		break;
	case ETRGraphicsQuality::Landscape:
		Level = Levels.LandscapeQuality;
		break;
	default:
		ensureMsgf(false, TEXT("TRSettings: unsupported graphics quality enum value"));
		break;
	}

	return static_cast<ETRQualityLevel>(FMath::Clamp(Level, 0, 4));
}

// OS 모니터 정보를 UI 표시 문자열 목록으로 변환
TArray<FString> UTRSettingsSubsystem::GetAvailableMonitorOptions() const
{
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	TArray<FString> Result;
	for (int32 Index = 0; Index < Metrics.MonitorInfo.Num(); ++Index)
	{
		const FMonitorInfo& Monitor = Metrics.MonitorInfo[Index];
		const FString Name = Monitor.FriendlyName.IsEmpty() ? Monitor.Name : Monitor.FriendlyName;
		Result.Add(FString::Printf(TEXT("%d. %s (%d x %d)%s"), Index + 1, *Name, Monitor.NativeWidth, Monitor.NativeHeight, Monitor.bIsPrimary ? TEXT(" [Primary]") : TEXT("")));
	}
	return Result;
}

// 저장된 ID/Index에서 가장 가까운 현재 모니터 선택지 반환
FString UTRSettingsSubsystem::GetCurrentMonitorOption() const
{
	const TArray<FString> Options = GetAvailableMonitorOptions();
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	const int32 Index = Metrics.GetClosestMonitorFromIDAndIndex(GetDisplayID(), GetDisplayIndex());
	return Options.IsValidIndex(Index) ? Options[Index] : (Options.IsEmpty() ? FString() : Options[0]);
}

FString UTRSettingsSubsystem::GetDisplayID() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetDisplayID() : FString();
}

int32 UTRSettingsSubsystem::GetDisplayIndex() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetDisplayIndex() : -1;
}

// 유효한 양수 해상도만 Preview 값으로 등록
void UTRSettingsSubsystem::SetScreenResolution(const FIntPoint Value)
{
	if (Value.X > 0 && Value.Y > 0)
	{
		if (UTRGameUserSettings* Settings = GetUserSettings())
		{
			Settings->SetScreenResolution(Value);
		}
	}
}

// 창모드 설정, UI Index를 EWindowMode 범위로 보정해 등록
void UTRSettingsSubsystem::SetWindowMode(const int32 Value)
{
	if (UTRGameUserSettings* Settings = GetUserSettings())
	{
		const int32 Clamped = FMath::Clamp(Value, 0, 2);
		Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(Clamped));
	}
}

// 수직동기화 설정
void UTRSettingsSubsystem::SetVSyncEnabled(const bool bValue)
{
	if (UTRGameUserSettings* Settings = GetUserSettings())
	{
		Settings->SetVSyncEnabled(bValue);
	}
}

// DRS 설정
void UTRSettingsSubsystem::SetDynamicResolutionEnabled(const bool bValue)
{
	if (UTRGameUserSettings* Settings = GetUserSettings())
	{
		Settings->SetDynamicResolutionEnabled(bValue);
	}
}

// FPS 제한 설정, 0은 Unlimited로 사용하며 음수 입력 차단
void UTRSettingsSubsystem::SetFrameRateLimit(const float Value)
{
	if (UTRGameUserSettings* Settings = GetUserSettings())
	{
		Settings->SetFrameRateLimit(FMath::Max(0.0f, Value));
	}
}

void UTRSettingsSubsystem::SetResolutionScale(const float Value)
{
	if (UTRGameUserSettings* Settings = GetUserSettings())
	{
		Settings->SetResolutionScaleValueEx(Value);
	}
}

// 전체 Scalability 단계를 Low~Cinematic 범위로 등록
void UTRSettingsSubsystem::SetOverallQuality(const int32 Value)
{
	if (UTRGameUserSettings* Settings = GetUserSettings())
	{
		Settings->SetOverallScalabilityLevel(FMath::Clamp(Value, 0, 4));
	}
}

// 열거형으로 지정한 개별 Scalability 품질 단계 변경
void UTRSettingsSubsystem::SetQualityLevel(const ETRGraphicsQuality Quality, const ETRQualityLevel Value)
{
	UTRGameUserSettings* Settings = GetUserSettings();
	if (!Settings)
	{
		return;
	}

	const int32 Level = FMath::Clamp(static_cast<int32>(Value), 0, 4);
	Scalability::FQualityLevels& Levels = Settings->ScalabilityQuality;
	switch (Quality)
	{
	case ETRGraphicsQuality::ViewDistance:
		Levels.ViewDistanceQuality = Level;
		break;
	case ETRGraphicsQuality::AntiAliasing:
		Levels.AntiAliasingQuality = Level;
		break;
	case ETRGraphicsQuality::Shadow:
		Levels.ShadowQuality = Level;
		break;
	case ETRGraphicsQuality::GlobalIllumination:
		Levels.GlobalIlluminationQuality = Level;
		break;
	case ETRGraphicsQuality::Reflection:
		Levels.ReflectionQuality = Level;
		break;
	case ETRGraphicsQuality::PostProcess:
		Levels.PostProcessQuality = Level;
		break;
	case ETRGraphicsQuality::Texture:
		Levels.TextureQuality = Level;
		break;
	case ETRGraphicsQuality::Effects:
		Levels.EffectsQuality = Level;
		break;
	case ETRGraphicsQuality::Foliage:
		Levels.FoliageQuality = Level;
		break;
	case ETRGraphicsQuality::Shading:
		Levels.ShadingQuality = Level;
		break;
	case ETRGraphicsQuality::Landscape:
		Levels.LandscapeQuality = Level;
		break;
	default:
		ensureMsgf(false, TEXT("TRSettings: unsupported graphics quality enum value"));
		break;
	}
}

// UI 표시 문자열의 앞쪽 번호를 실제 모니터 ID와 Index로 변환
void UTRSettingsSubsystem::SetMonitorOption(const FString& Option)
{
	FString IndexText;
	FString Description;
	if (!Option.Split(TEXT("."), &IndexText, &Description))
	{
		return;
	}
	const int32 Index = FCString::Atoi(*IndexText) - 1;
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	if (Metrics.MonitorInfo.IsValidIndex(Index))
	{
		SetDisplayProperties(Metrics.MonitorInfo[Index].ID, Index);
	}
}

void UTRSettingsSubsystem::SetDisplayProperties(const FString& ID, const int32 Index)
{
	if (UTRGameUserSettings* Settings = GetUserSettings())
	{
		Settings->SetDisplayProperties(ID, Index);
	}
}

// CPU/GPU 벤치마크 결과를 Scalability Preview에 적용하고 결과 유효성 반환
bool UTRSettingsSubsystem::RunHardwareBenchmark(const int32 WorkScale)
{
	UTRGameUserSettings* Settings = GetUserSettings();
	if (!Settings)
	{
		return false;
	}
	Settings->RunHardwareBenchmark(FMath::Clamp(WorkScale, 1, 100));
	Settings->ApplyNonResolutionSettings();
	return FMath::IsFinite(Settings->GetLastCPUBenchmarkResult()) && Settings->GetLastCPUBenchmarkResult() >= 0.0f
		&& FMath::IsFinite(Settings->GetLastGPUBenchmarkResult()) && Settings->GetLastGPUBenchmarkResult() >= 0.0f;
}

//--- 오디오 및 조작 설정 조회 ---
float UTRSettingsSubsystem::GetMasterVolume() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetMasterVolume() : 1.0f;
}

float UTRSettingsSubsystem::GetMusicVolume() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetMusicVolume() : 1.0f;
}

float UTRSettingsSubsystem::GetEffectsVolume() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetEffectsVolume() : 1.0f;
}

float UTRSettingsSubsystem::GetLookSensitivity() const
{
	const UTRGameUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetLookSensitivity() : 1.0f;
}

// 오디오 값은 설정 객체 변경과 동시에 SoundMix Override로 Preview
void UTRSettingsSubsystem::SetMasterVolume(const float Value)
{
	if (UTRGameUserSettings* S = GetUserSettings())
	{
		S->SetMasterVolume(Value);
		ApplySoundSettings();
	}
}

void UTRSettingsSubsystem::SetMusicVolume(const float Value)
{
	if (UTRGameUserSettings* S = GetUserSettings())
	{
		S->SetMusicVolume(Value);
		ApplySoundSettings();
	}
}

void UTRSettingsSubsystem::SetEffectsVolume(const float Value)
{
	if (UTRGameUserSettings* S = GetUserSettings())
	{
		S->SetEffectsVolume(Value);
		ApplySoundSettings();
	}
}
// 감도 변경 즉시 캐릭터 입력 계층에 알림
void UTRSettingsSubsystem::SetLookSensitivity(const float Value)
{
	if (UTRGameUserSettings* S = GetUserSettings())
	{
		S->SetLookSensitivity(Value);
		OnLookSensitivityChanged.Broadcast(S->GetLookSensitivity());
	}
}

// Preview 값을 런타임에 적용하고 요청 시 GameUserSettings.ini에 확정 저장
bool UTRSettingsSubsystem::ApplySettings(const bool bSaveSettings)
{
	UTRGameUserSettings* Settings = GetUserSettings();
	if (!Settings)
	{
		return false;
	}
	Settings->ValidateSettings();
	ApplyRuntimeSettingsWithoutSaving();
	if (bSaveSettings)
	{
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
		PersistedScalabilityQuality = Settings->ScalabilityQuality;
	}
	const bool bAudioApplied = ApplySoundSettings();
	OnLookSensitivityChanged.Broadcast(Settings->GetLookSensitivity());
	return bAudioApplied;
}

// ini 저장값을 다시 읽어 미저장 변경을 폐기하고 런타임 상태 복원
bool UTRSettingsSubsystem::ReloadSettings()
{
	UTRGameUserSettings* Settings = GetUserSettings();
	if (!Settings)
	{
		return false;
	}
	Settings->LoadSettings(true);
	Settings->ValidateSettings();
	Settings->ScalabilityQuality = PersistedScalabilityQuality;
	ApplyRuntimeSettingsWithoutSaving();
	const bool bAudioApplied = ApplySoundSettings();
	OnLookSensitivityChanged.Broadcast(Settings->GetLookSensitivity());
	return bAudioApplied;
}

// 프로젝트 기본값을 Preview하되 Apply 전까지 ini에는 저장하지 않음
bool UTRSettingsSubsystem::ResetToDefaults()
{
	UTRGameUserSettings* Settings = GetUserSettings();
	if (!Settings)
	{
		return false;
	}
	Settings->SetToDefaults();
	Settings->ValidateSettings();
	const bool bAudioApplied = ApplySoundSettings();
	OnLookSensitivityChanged.Broadcast(Settings->GetLookSensitivity());
	return bAudioApplied;
}

// UGameUserSettings의 해상도/비해상도 적용 경로만 실행하고 저장 생략
void UTRSettingsSubsystem::ApplyRuntimeSettingsWithoutSaving()
{
	if (UTRGameUserSettings* Settings = GetUserSettings())
	{
		Settings->ApplyResolutionSettings(false);
		Settings->ApplyNonResolutionSettings();
	}
}

// SoundMix는 한 번만 Push하고 세 SoundClass의 현재 볼륨 Override 갱신
bool UTRSettingsSubsystem::ApplySoundSettings(UWorld* WorldOverride)
{
	UWorld* World = WorldOverride ? WorldOverride : GetWorld();
	UTRGameUserSettings* Settings = GetUserSettings();
	if (!World || World->GetGameInstance() != GetGameInstance() || !Settings)
	{
		return false;
	}
	const UTRSettingsProjectSettings* ProjectSettings = GetDefault<UTRSettingsProjectSettings>();
	LoadedSoundMix = ProjectSettings->SettingsSoundMix.LoadSynchronous();
	LoadedMasterSoundClass = ProjectSettings->MasterSoundClass.LoadSynchronous();
	LoadedMusicSoundClass = ProjectSettings->MusicSoundClass.LoadSynchronous();
	LoadedEffectsSoundClass = ProjectSettings->EffectsSoundClass.LoadSynchronous();
	if (!LoadedSoundMix || !LoadedMasterSoundClass || !LoadedMusicSoundClass || !LoadedEffectsSoundClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("TRSettings: audio assets are missing"));
		return false;
	}
	if (!bSoundMixPushed)
	{
		UGameplayStatics::PushSoundMixModifier(World, LoadedSoundMix);
		bSoundMixPushed = true;
	}
	UGameplayStatics::SetSoundMixClassOverride(World, LoadedSoundMix, LoadedMasterSoundClass, Settings->GetMasterVolume(), 1.0f, 0.0f, true);
	UGameplayStatics::SetSoundMixClassOverride(World, LoadedSoundMix, LoadedMusicSoundClass, Settings->GetMusicVolume(), 1.0f, 0.0f, true);
	UGameplayStatics::SetSoundMixClassOverride(World, LoadedSoundMix, LoadedEffectsSoundClass, Settings->GetEffectsVolume(), 1.0f, 0.0f, true);
	return true;
}

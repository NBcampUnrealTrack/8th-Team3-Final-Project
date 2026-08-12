// TRSettingsSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Scalability.h"
#include "Settings/TRSettingsTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TRSettingsSubsystem.generated.h"

class USoundClass;
class USoundMix;
class UTRGameUserSettings;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTRLookSensitivityChanged, float, NewSensitivity);

// UTRGameUserSettings 및 프로젝트 오디오 에셋과 UI 사이에서 설정 조회, Preview, 적용, 저장을 담당하는 GameInstance Subsystem
UCLASS()
class TR_API UTRSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 저장 설정 로드와 런타임 반영, delegate 및 SoundMix 수명주기 관리
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//--- 설정 객체 및 그래픽 설정 조회 ---
	UFUNCTION(BlueprintPure, Category = "TR|Settings")
	UTRGameUserSettings* GetUserSettings() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics")
	TArray<FIntPoint> GetAvailableResolutions() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics")
	FIntPoint GetScreenResolution() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics")
	int32 GetWindowMode() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics")
	bool IsVSyncEnabled() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics")
	bool IsDynamicResolutionEnabled() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics")
	float GetFrameRateLimit() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics")
	float GetResolutionScale() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics")
	int32 GetOverallQuality() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics")
	ETRQualityLevel GetQualityLevel(ETRGraphicsQuality Quality) const;

	//--- 모니터 설정 조회 ---
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|Display")
	TArray<FString> GetAvailableMonitorOptions() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|Display")
	FString GetCurrentMonitorOption() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|Display")
	FString GetDisplayID() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|Display")
	int32 GetDisplayIndex() const;

	//--- HDR 설정 조회 ---
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|HDR")
	bool SupportsHDRDisplayOutput() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|HDR")
	bool IsHDREnabled() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|HDR")
	int32 GetHDRDisplayNits() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|HDR")
	float GetHDRPaperWhiteNits() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|HDR")
	float GetHDRUILuminanceNits() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|HDR")
	bool IsHDRUILuminanceSeparate() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Graphics|HDR")
	bool IsHDRCalibrationUsed() const;

	//--- 그래픽 설정 변경 ---
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics")
	void SetScreenResolution(FIntPoint Value);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics")
	void SetWindowMode(int32 Value);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics")
	void SetVSyncEnabled(bool bValue);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics")
	void SetDynamicResolutionEnabled(bool bValue);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics")
	void SetFrameRateLimit(float Value);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics")
	void SetResolutionScale(float Value);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics")
	void SetOverallQuality(int32 Value);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics")
	void SetQualityLevel(ETRGraphicsQuality Quality, ETRQualityLevel Value);

	//--- 모니터 설정 변경 ---
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics|Display")
	void SetMonitorOption(const FString& Option);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics|Display")
	void SetDisplayProperties(const FString& ID, int32 Index);

	//--- HDR 설정 변경 ---
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics|HDR")
	void SetHDREnabled(bool bValue);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics|HDR")
	void SetHDRDisplayNits(int32 Value);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics|HDR")
	void SetHDRPaperWhiteNits(float Value);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics|HDR")
	void SetHDRUILuminanceNits(float Value);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics|HDR")
	void SetHDRUILuminanceSeparate(bool bValue);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics|HDR")
	void SetHDRCalibrationUsed(bool bValue);

	// 하드웨어 성능 측정 후 권장 Scalability 값을 Preview 상태로 적용
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Graphics")
	bool RunHardwareBenchmark(int32 WorkScale = 10);

	//--- 오디오 설정 ---
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Audio")
	float GetMasterVolume() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Audio")
	float GetMusicVolume() const;
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Audio")
	float GetEffectsVolume() const;
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Audio")
	void SetMasterVolume(float Value);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Audio")
	void SetMusicVolume(float Value);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Audio")
	void SetEffectsVolume(float Value);

	//--- 조작 설정 ---
	UFUNCTION(BlueprintPure, Category = "TR|Settings|Control")
	float GetLookSensitivity() const;
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|Control")
	void SetLookSensitivity(float Value);

	//--- 설정 적용 및 저장 ---
	// ApplySettings: 현재 Preview 값을 런타임에 적용하고 선택적으로 ini에 저장
	// ReloadSettings: 저장하지 않은 변경을 폐기하고 마지막 저장값 복원
	// ResetToDefaults: 기본값을 Preview 상태로 적용하며 즉시 저장하지 않음
	UFUNCTION(BlueprintCallable, Category = "TR|Settings")
	bool ApplySettings(bool bSaveSettings = true);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings")
	bool ReloadSettings();
	UFUNCTION(BlueprintCallable, Category = "TR|Settings")
	bool ResetToDefaults();

	// 감도 Preview 또는 복원 시 로컬 플레이어 입력 계층에 변경 알림
	UPROPERTY(BlueprintAssignable, Category = "TR|Settings")
	FTRLookSensitivityChanged OnLookSensitivityChanged;

private:
	// 맵 로드 후 World에 저장된 오디오 설정 적용
	void HandlePostLoadMap(UWorld* LoadedWorld);

	// SoundMix와 SoundClass Override를 현재 World에 적용
	bool ApplySoundSettings(UWorld* WorldOverride = nullptr);

	// ini 저장 없이 해상도와 비해상도 그래픽 설정을 런타임에 적용
	void ApplyRuntimeSettingsWithoutSaving();

	//--- 로드된 오디오 에셋 ---
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> LoadedSoundMix;
	UPROPERTY(Transient)
	TObjectPtr<USoundClass> LoadedMasterSoundClass;
	UPROPERTY(Transient)
	TObjectPtr<USoundClass> LoadedMusicSoundClass;
	UPROPERTY(Transient)
	TObjectPtr<USoundClass> LoadedEffectsSoundClass;

	//--- 런타임 저장 상태 ---
	Scalability::FQualityLevels PersistedScalabilityQuality;
	FDelegateHandle PostLoadMapHandle;
	bool bSoundMixPushed = false;
};

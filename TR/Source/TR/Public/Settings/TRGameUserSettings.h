// TRGameUserSettings.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "TRGameUserSettings.generated.h"

// 엔진 기본 그래픽 설정과 프로젝트 전용 오디오/조작 설정을 GameUserSettings.ini에 저장하는 클래스
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class TR_API UTRGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	// 프로젝트 기본값 적용 및 ini에서 불러온 값의 유효 범위 검증
	virtual void SetToDefaults() override;
	virtual void ValidateSettings() override;

	//--- 오디오 설정 조회 ---
	UFUNCTION(BlueprintPure, Category = "TR|Settings")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category = "TR|Settings")
	float GetMusicVolume() const { return MusicVolume; }

	UFUNCTION(BlueprintPure, Category = "TR|Settings")
	float GetEffectsVolume() const { return EffectsVolume; }

	//--- 조작 설정 조회 ---
	UFUNCTION(BlueprintPure, Category = "TR|Settings")
	float GetLookSensitivity() const { return LookSensitivity; }

	//--- 오디오 설정 변경 ---
	UFUNCTION(BlueprintCallable, Category = "TR|Settings")
	void SetMasterVolume(float Value);

	UFUNCTION(BlueprintCallable, Category = "TR|Settings")
	void SetMusicVolume(float Value);

	UFUNCTION(BlueprintCallable, Category = "TR|Settings")
	void SetEffectsVolume(float Value);

	//--- 조작 설정 변경 ---
	UFUNCTION(BlueprintCallable, Category = "TR|Settings")
	void SetLookSensitivity(float Value);

private:
	//--- GameUserSettings.ini 저장값 ---
	UPROPERTY(Config)
	float MasterVolume = 1.0f;

	UPROPERTY(Config)
	float MusicVolume = 1.0f;

	UPROPERTY(Config)
	float EffectsVolume = 1.0f;

	UPROPERTY(Config)
	float LookSensitivity = 1.0f;
};

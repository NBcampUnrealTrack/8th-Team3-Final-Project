// TRGameUserSettings.cpp

#include "Settings/TRGameUserSettings.h"

// 프로젝트 전용 설정의 초기 기본값
namespace TRSettingsDefaults
{
	constexpr float Volume = 1.0f;
	constexpr float LookSensitivity = 1.0f;
}

// 엔진 기본 그래픽값과 프로젝트 전용 오디오/조작값 초기화
void UTRGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MasterVolume = TRSettingsDefaults::Volume;
	MusicVolume = TRSettingsDefaults::Volume;
	EffectsVolume = TRSettingsDefaults::Volume;
	LookSensitivity = TRSettingsDefaults::LookSensitivity;
}

// ini 손상이나 외부 수정으로 범위를 벗어난 값을 안전 범위로 보정
void UTRGameUserSettings::ValidateSettings()
{
	Super::ValidateSettings();
	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.0f, 1.0f);
	EffectsVolume = FMath::Clamp(EffectsVolume, 0.0f, 1.0f);
	LookSensitivity = FMath::Clamp(LookSensitivity, 0.1f, 5.0f);
}

// 마스터 볼륨을 0~1 범위로 저장
void UTRGameUserSettings::SetMasterVolume(const float Value)
{
	MasterVolume = FMath::Clamp(Value, 0.0f, 1.0f);
}

// 배경음 볼륨을 0~1 범위로 저장
void UTRGameUserSettings::SetMusicVolume(const float Value)
{
	MusicVolume = FMath::Clamp(Value, 0.0f, 1.0f);
}

// 효과음 볼륨을 0~1 범위로 저장
void UTRGameUserSettings::SetEffectsVolume(const float Value)
{
	EffectsVolume = FMath::Clamp(Value, 0.0f, 1.0f);
}

// 시점 감도를 UI 제공 범위인 0.1~5로 저장
void UTRGameUserSettings::SetLookSensitivity(const float Value)
{
	LookSensitivity = FMath::Clamp(Value, 0.1f, 5.0f);
}

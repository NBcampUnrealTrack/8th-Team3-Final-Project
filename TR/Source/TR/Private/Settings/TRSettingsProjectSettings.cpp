// TRSettingsProjectSettings.cpp

#include "Settings/TRSettingsProjectSettings.h"

// 현재 설정 테스트 에셋을 기본 참조로 등록하며 Project Settings에서 교체 가능
UTRSettingsProjectSettings::UTRSettingsProjectSettings()
{
	SettingsSoundMix = TSoftObjectPtr<USoundMix>(FSoftObjectPath(TEXT("/Game/Developers/Jaeseong/Settings/Audio/SM_TRSettings.SM_TRSettings")));
	MasterSoundClass = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Game/Developers/Jaeseong/Settings/Audio/SC_TRMaster.SC_TRMaster")));
	MusicSoundClass = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Game/Developers/Jaeseong/Settings/Audio/SC_TRMusic.SC_TRMusic")));
	EffectsSoundClass = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Game/Developers/Jaeseong/Settings/Audio/SC_TREffects.SC_TREffects")));
}

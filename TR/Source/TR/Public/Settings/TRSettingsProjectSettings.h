// TRSettingsProjectSettings.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "TRSettingsProjectSettings.generated.h"

class USoundClass;
class USoundMix;

// 설정 시스템에서 사용할 오디오 에셋을 Project Settings에 노출하는 프로젝트 설정
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "TR Settings"))
class TR_API UTRSettingsProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTRSettingsProjectSettings();

	//--- 설정 Preview 오디오 에셋 ---
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundMix> SettingsSoundMix;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundClass> EffectsSoundClass;
};

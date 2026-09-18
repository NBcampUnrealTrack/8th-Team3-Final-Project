#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPtr.h"
#include "BGSystemAudioTypes.generated.h"

class USoundBase;
class USoundConcurrency;

/** 출력 버스. 실제 볼륨 분리는 SoundClass가 담당하고 이 값은 카탈로그 분류용이다. */
UENUM(BlueprintType)
enum class EBGSystemAudioCategory : uint8
{
	UI,
	System,
	Music
};

/** 같은 태그가 다시 들어왔을 때의 수명 정책. */
UENUM(BlueprintType)
enum class EBGSystemAudioPlaybackPolicy : uint8
{
	/** 매 요청마다 독립 재생. 끝나면 자동 정리된다. */
	OneShot,
	/** 태그당 하나만 유지. 다시 요청하면 이미 재생 중인 컴포넌트를 반환한다. */
	Singleton,
	/** 태그로 명시적으로 중단하는 장기 재생. SoundCue/MetaSound 자체도 Loop 설정이어야 한다. */
	Loop
};

/**
 * DT_BGSystemAudio 한 행. Row 이름은 Audio.* GameplayTag 문자열과 같아야 한다.
 * Niagara 및 Montage 내부 사운드는 여기서 관리하지 않는다.
 */
USTRUCT(BlueprintType)
struct BG_API FBGSystemAudioRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	EBGSystemAudioCategory Category = EBGSystemAudioCategory::UI;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	EBGSystemAudioPlaybackPolicy PlaybackPolicy = EBGSystemAudioPlaybackPolicy::OneShot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInDuration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutDuration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundConcurrency> Concurrency;

	/** BGM처럼 맵 트래블 뒤에도 살아 있어야 하는 사운드. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	bool bPersistAcrossLevelTransition = false;

	/** Subsystem 초기화 때 Sound/Concurrency를 미리 로드해 첫 입력 지연을 없앤다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	bool bPreload = false;
};

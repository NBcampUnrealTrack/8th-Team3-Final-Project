#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BGEnvironmentalSoundActor.generated.h"

class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/** 한 종류의 환경음을 언제, 어느 거리에서 재생할지 정의한다. */
USTRUCT(BlueprintType)
struct BG_API FBGEnvironmentalSoundEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio|Timing",
		meta = (ClampMin = "0.0", Units = "s"))
	float MinInterval = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio|Timing",
		meta = (ClampMin = "0.0", Units = "s"))
	float MaxInterval = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio|Location",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MinRadius = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio|Location",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MaxRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio|Playback",
		meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio|Playback",
		meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.f;

	/** 비어 있으면 Sound 에셋 자체의 감쇠 설정을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio|Playback")
	TObjectPtr<USoundAttenuation> AttenuationSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio|Playback")
	TObjectPtr<USoundConcurrency> ConcurrencySettings;
};

/**
 * 항목별 랜덤 간격으로 액터 주변 XY 원환 영역에 단발 환경음을 재생한다.
 * 각 항목은 독립 타이머를 사용하므로 서로 다른 환경음이 같은 시점에 겹칠 수 있다.
 */
UCLASS(Blueprintable)
class BG_API ABGEnvironmentalSoundActor : public AActor
{
	GENERATED_BODY()

public:
	ABGEnvironmentalSoundActor();

	UFUNCTION(BlueprintCallable, Category = "BG|Environment Audio")
	void StartEnvironmentSounds();

	UFUNCTION(BlueprintCallable, Category = "BG|Environment Audio")
	void StopEnvironmentSounds();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Environment Audio")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio",
		meta = (TitleProperty = "Sound"))
	TArray<FBGEnvironmentalSoundEntry> Sounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment Audio")
	bool bAutoStart = true;

private:
	void ScheduleSound(int32 EntryIndex);
	void HandleSoundTimer(int32 EntryIndex);
	FVector MakeRandomSoundLocation(const FBGEnvironmentalSoundEntry& Entry) const;

	TArray<FTimerHandle> SoundTimerHandles;
};

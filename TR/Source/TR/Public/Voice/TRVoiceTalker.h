// TRVoiceTalker.h
// 원격 플레이어 한 명의 음성 재생을 붙잡는다. 개인별 음량과 음원 위치는 여기서 적용된다.

#pragma once

#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "TRVoiceTalker.generated.h"

class APawn;
class APlayerState;
class UAudioComponent;
class USoundAttenuation;
class USoundClass;

/** PlayerState 에 붙어 그 플레이어 음성의 오디오 컴포넌트에 음량·사운드 클래스·3D 위치를 적용한다. */
UCLASS()
class TR_API UTRVoiceTalker : public UVOIPTalker
{
	GENERATED_BODY()

public:
	UTRVoiceTalker(const FObjectInitializer& ObjectInitializer);

	/** 전체 음성 음량 x 개인 음량. 재생 중이면 즉시, 아니면 다음 발화 때 적용된다. */
	void ApplyVolume(float InVolume);

	/** 재생에 쓸 사운드 클래스. 프로젝트 마스터 볼륨 아래에 들어가게 한다. */
	void SetSoundClass(USoundClass* InSoundClass);

	/** 공간 음성 설정. 켜면 음원을 그 플레이어의 폰에 붙이고 감쇠를 건다. 재생 중이면 즉시 옮긴다. */
	void SetSpatial(bool bInSpatial, USoundAttenuation* InAttenuation);

	virtual void OnTalkingBegin(UAudioComponent* AudioComponent) override;

	virtual void OnTalkingEnd() override;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 폰이 바뀌면(리스폰 등) 음원 부착 대상을 갱신하고 재생 중인 소리도 옮긴다. */
	UFUNCTION()
	void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	void RefreshAttachTarget();

	TWeakObjectPtr<UAudioComponent> ActiveAudio;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> SoundClass;

	float Volume = 1.f;

	bool bSpatial = false;
};

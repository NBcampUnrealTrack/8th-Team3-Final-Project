// TRVoiceTalker.h
// 원격 플레이어 한 명의 음성 재생을 붙잡는다. 개인별 음량은 여기서 적용된다.

#pragma once

#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "TRVoiceTalker.generated.h"

class UAudioComponent;

/** PlayerState 에 붙어 그 플레이어 음성의 오디오 컴포넌트에 음량을 적용한다. */
UCLASS()
class TR_API UTRVoiceTalker : public UVOIPTalker
{
	GENERATED_BODY()

public:
	/** 전체 음성 음량 x 개인 음량. 재생 중이면 즉시, 아니면 다음 발화 때 적용된다. */
	void ApplyVolume(float InVolume);

	virtual void OnTalkingBegin(UAudioComponent* AudioComponent) override;

	virtual void OnTalkingEnd() override;

private:
	TWeakObjectPtr<UAudioComponent> ActiveAudio;

	float Volume = 1.f;
};

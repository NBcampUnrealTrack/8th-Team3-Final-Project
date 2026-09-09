// TRVoiceTalker.h
// 원격 플레이어 한 명의 음성 재생을 붙잡는다. 개인별 음량은 여기서 적용된다.

#pragma once

#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "TRVoiceTalker.generated.h"

class UAudioComponent;

/**
 * PlayerState 에 붙는다. 엔진이 그 플레이어의 음성을 재생하기 시작할 때
 * OnTalkingBegin 으로 오디오 컴포넌트를 넘겨 주므로, 거기에 음량을 건다.
 */
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

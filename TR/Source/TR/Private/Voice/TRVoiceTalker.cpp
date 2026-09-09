// TRVoiceTalker.cpp

#include "Voice/TRVoiceTalker.h"

#include "Components/AudioComponent.h"

void UTRVoiceTalker::ApplyVolume(float InVolume)
{
	Volume = FMath::Max(0.f, InVolume);

	if (UAudioComponent* Audio = ActiveAudio.Get())
	{
		Audio->SetVolumeMultiplier(Volume);
	}
}

void UTRVoiceTalker::OnTalkingBegin(UAudioComponent* AudioComponent)
{
	// 엔진이 토커마다 오디오 컴포넌트를 만들었다 없앴다 하므로 발화마다 다시 건다.
	ActiveAudio = AudioComponent;
	if (AudioComponent != nullptr)
	{
		AudioComponent->SetVolumeMultiplier(Volume);
	}

	Super::OnTalkingBegin(AudioComponent);
}

void UTRVoiceTalker::OnTalkingEnd()
{
	ActiveAudio.Reset();
	Super::OnTalkingEnd();
}

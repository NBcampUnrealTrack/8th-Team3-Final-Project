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
	// 발화마다 넘어오는 오디오 컴포넌트에 음량을 건다.
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

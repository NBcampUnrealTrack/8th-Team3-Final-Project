// TRVoiceTalker.cpp

#include "Voice/TRVoiceTalker.h"

#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Sound/SoundAttenuation.h"

UTRVoiceTalker::UTRVoiceTalker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.5f;
}

void UTRVoiceTalker::ApplyVolume(float InVolume)
{
	Volume = FMath::Max(0.f, InVolume);

	if (UAudioComponent* Audio = ActiveAudio.Get())
	{
		Audio->SetVolumeMultiplier(Volume);
	}
}

void UTRVoiceTalker::SetSpatial(bool bInSpatial, USoundAttenuation* InAttenuation)
{
	bSpatial = bInSpatial;
	Attenuation = bInSpatial ? InAttenuation : nullptr;
	RefreshAttachTarget();
}

void UTRVoiceTalker::RefreshAttachTarget()
{
	USceneComponent* Target = nullptr;
	if (bSpatial)
	{
		const APlayerState* PS = GetOwner<APlayerState>();
		const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		Target = Pawn ? Pawn->GetRootComponent() : nullptr;
	}

	Settings.ComponentToAttachTo = Target;
	Settings.AttenuationSettings = (Target != nullptr) ? Attenuation.Get() : nullptr;
}

void UTRVoiceTalker::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bSpatial)
	{
		RefreshAttachTarget();
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

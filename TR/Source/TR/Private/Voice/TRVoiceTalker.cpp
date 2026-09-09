// TRVoiceTalker.cpp

#include "Voice/TRVoiceTalker.h"

#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"

UTRVoiceTalker::UTRVoiceTalker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTRVoiceTalker::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerState* PS = GetOwner<APlayerState>())
	{
		PS->OnPawnSet.AddDynamic(this, &UTRVoiceTalker::HandlePawnSet);
	}
}

void UTRVoiceTalker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerState* PS = GetOwner<APlayerState>())
	{
		PS->OnPawnSet.RemoveDynamic(this, &UTRVoiceTalker::HandlePawnSet);
	}

	Super::EndPlay(EndPlayReason);
}

void UTRVoiceTalker::ApplyVolume(float InVolume)
{
	Volume = FMath::Max(0.f, InVolume);

	if (UAudioComponent* Audio = ActiveAudio.Get())
	{
		Audio->SetVolumeMultiplier(Volume);
	}
}

void UTRVoiceTalker::SetSoundClass(USoundClass* InSoundClass)
{
	SoundClass = InSoundClass;

	if (UAudioComponent* Audio = ActiveAudio.Get())
	{
		Audio->SoundClassOverride = SoundClass;
	}
}

void UTRVoiceTalker::SetSpatial(bool bInSpatial, USoundAttenuation* InAttenuation)
{
	bSpatial = bInSpatial;
	Attenuation = bInSpatial ? InAttenuation : nullptr;
	RefreshAttachTarget();
}

void UTRVoiceTalker::HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
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

	// 재생 중인 소리는 다음 발화를 기다리지 않고 바로 새 폰으로 옮긴다.
	if (UAudioComponent* Audio = ActiveAudio.Get())
	{
		if (Target != nullptr)
		{
			Audio->AttachToComponent(Target, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		else
		{
			Audio->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
}

void UTRVoiceTalker::OnTalkingBegin(UAudioComponent* AudioComponent)
{
	// 발화마다 넘어오는 오디오 컴포넌트에 음량과 사운드 클래스를 건다.
	ActiveAudio = AudioComponent;
	if (AudioComponent != nullptr)
	{
		AudioComponent->SetVolumeMultiplier(Volume);
		AudioComponent->SoundClassOverride = SoundClass;
	}

	Super::OnTalkingBegin(AudioComponent);
}

void UTRVoiceTalker::OnTalkingEnd()
{
	ActiveAudio.Reset();
	Super::OnTalkingEnd();
}

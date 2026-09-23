#include "Audio/BGEnvironmentalSoundActor.h"

#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "TimerManager.h"

ABGEnvironmentalSoundActor::ABGEnvironmentalSoundActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

// 시작하면서 사운드 반복 설정
void ABGEnvironmentalSoundActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStart)
	{
		StartEnvironmentSounds();
	}
}

// 종료하면서 사운드 반복 타이머 해제
void ABGEnvironmentalSoundActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopEnvironmentSounds();
	Super::EndPlay(EndPlayReason);
}

// 주어진 사운드 에셋들에 대해 타이머 세팅 진행
void ABGEnvironmentalSoundActor::StartEnvironmentSounds()
{
	StopEnvironmentSounds();
	SoundTimerHandles.SetNum(Sounds.Num());

	for (int32 EntryIndex = 0; EntryIndex < Sounds.Num(); ++EntryIndex)
	{
		ScheduleSound(EntryIndex);
	}
}

// 살아있는 재생 타이머들에 대해 초기화
void ABGEnvironmentalSoundActor::StopEnvironmentSounds()
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& TimerHandle : SoundTimerHandles)
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
	}

	SoundTimerHandles.Reset();
}

// 특정 사운드 에셋(인덱스)에 대해 다음 재생 타이머 세팅
void ABGEnvironmentalSoundActor::ScheduleSound(int32 EntryIndex)
{
	if (!Sounds.IsValidIndex(EntryIndex) || !SoundTimerHandles.IsValidIndex(EntryIndex))
	{
		return;
	}

	const FBGEnvironmentalSoundEntry& Entry = Sounds[EntryIndex];
	if (!Entry.Sound)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float MinDelay = FMath::Max(0.f, FMath::Min(Entry.MinInterval, Entry.MaxInterval));
	const float MaxDelay = FMath::Max(MinDelay, FMath::Max(Entry.MinInterval, Entry.MaxInterval));
	const float Delay = FMath::FRandRange(MinDelay, MaxDelay);

	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &ABGEnvironmentalSoundActor::HandleSoundTimer, EntryIndex);
	World->GetTimerManager().SetTimer(
		SoundTimerHandles[EntryIndex], TimerDelegate, Delay, /*bLoop=*/false);
}

// 특정 사운드 에셋(인덱스)에 대해 재생 및 다음 타이머 예약
void ABGEnvironmentalSoundActor::HandleSoundTimer(int32 EntryIndex)
{
	if (!Sounds.IsValidIndex(EntryIndex))
	{
		return;
	}

	const FBGEnvironmentalSoundEntry& Entry = Sounds[EntryIndex];
	if (Entry.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			Entry.Sound,
			MakeRandomSoundLocation(Entry),
			FRotator::ZeroRotator,
			Entry.VolumeMultiplier,
			Entry.PitchMultiplier,
			/*StartTime=*/0.f,
			Entry.AttenuationSettings,
			Entry.ConcurrencySettings,
			this);
	}

	ScheduleSound(EntryIndex);
}

// 주어진 조건에 대해 랜덤한 스폰 위치 획득
FVector ABGEnvironmentalSoundActor::MakeRandomSoundLocation(
	const FBGEnvironmentalSoundEntry& Entry) const
{
	const float MinRadius = FMath::Max(0.f, FMath::Min(Entry.MinRadius, Entry.MaxRadius));
	const float MaxRadius = FMath::Max(MinRadius, FMath::Max(Entry.MinRadius, Entry.MaxRadius));
	const float Radius = FMath::Sqrt(FMath::FRandRange(
		FMath::Square(MinRadius), FMath::Square(MaxRadius)));
	const float Angle = FMath::FRandRange(0.f, UE_TWO_PI);

	return GetActorLocation() + FVector(
		FMath::Cos(Angle) * Radius,
		FMath::Sin(Angle) * Radius,
		0.f);
}

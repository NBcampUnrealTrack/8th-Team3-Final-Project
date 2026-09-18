#include "Audio/BGAudioSubsystem.h"

#include "Audio/BGSystemAudioTypes.h"
#include "BGGameplayTags.h"
#include "Card/BGDataSettings.h"

#include "Components/AudioComponent.h"
#include "Components/Button.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"

DEFINE_LOG_CATEGORY_STATIC(LogBGAudio, Log, All);

UBGAudioSubsystem* UBGAudioSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UBGAudioSubsystem>() : nullptr;
}

void UBGAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UBGDataSettings* Settings = GetDefault<UBGDataSettings>();
	AudioTable = Settings ? Settings->AudioTable.LoadSynchronous() : nullptr;
	if (!AudioTable)
	{
		UE_LOG(LogBGAudio, Warning,
			TEXT("AudioTable 로드 실패 — UI/시스템/BGM 사운드가 재생되지 않습니다."));
		return;
	}

	TArray<FBGSystemAudioRow*> Rows;
	AudioTable->GetAllRows<FBGSystemAudioRow>(TEXT("BGAudioPreload"), Rows);
	for (const FBGSystemAudioRow* Row : Rows)
	{
		if (Row && Row->bPreload)
		{
			Row->Sound.LoadSynchronous();
			Row->Concurrency.LoadSynchronous();
		}
	}
}

void UBGAudioSubsystem::Deinitialize()
{
	for (const TPair<FGameplayTag, TObjectPtr<UAudioComponent>>& Pair : ActiveSounds)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->OnAudioFinished.RemoveDynamic(
				this, &UBGAudioSubsystem::HandleTrackedSoundFinished);
			Pair.Value->Stop();
		}
	}
	ActiveSounds.Reset();

	if (IsValid(CurrentMusic))
	{
		CurrentMusic->OnAudioFinished.RemoveDynamic(
			this, &UBGAudioSubsystem::HandleCurrentMusicFinished);
		CurrentMusic->Stop();
	}
	CurrentMusic = nullptr;
	CurrentMusicTag = FGameplayTag();
	AudioTable = nullptr;

	Super::Deinitialize();
}

const FBGSystemAudioRow* UBGAudioSubsystem::FindRow(FGameplayTag AudioTag) const
{
	if (!AudioTable || !AudioTag.IsValid())
	{
		return nullptr;
	}

	return AudioTable->FindRow<FBGSystemAudioRow>(
		AudioTag.GetTagName(), TEXT("BGSystemAudio"), /*bWarnIfRowMissing=*/false);
}

bool UBGAudioSubsystem::HasAudioRow(FGameplayTag AudioTag) const
{
	return FindRow(AudioTag) != nullptr;
}

UAudioComponent* UBGAudioSubsystem::CreateComponent(
	FGameplayTag AudioTag, const FBGSystemAudioRow& Row)
{
	if (Row.Sound.IsNull())
	{
		return nullptr; // Row만 미리 만들어 두고 아트 작업자가 나중에 채우는 것은 정상이다.
	}

	USoundBase* Sound = Row.Sound.LoadSynchronous();
	if (!Sound)
	{
		UE_LOG(LogBGAudio, Warning, TEXT("%s Sound 로드 실패: %s"),
			*AudioTag.ToString(), *Row.Sound.ToString());
		return nullptr;
	}

	USoundConcurrency* Concurrency = Row.Concurrency.IsNull()
		? nullptr
		: Row.Concurrency.LoadSynchronous();

	return UGameplayStatics::CreateSound2D(
		this,
		Sound,
		Row.VolumeMultiplier,
		Row.PitchMultiplier,
		/*StartTime=*/0.f,
		Concurrency,
		Row.bPersistAcrossLevelTransition,
		/*bAutoDestroy=*/Row.PlaybackPolicy == EBGSystemAudioPlaybackPolicy::OneShot);
}

UAudioComponent* UBGAudioSubsystem::Play(FGameplayTag AudioTag)
{
	const FBGSystemAudioRow* Row = FindRow(AudioTag);
	if (!Row)
	{
		return nullptr;
	}

	if (Row->Category == EBGSystemAudioCategory::Music)
	{
		return PlayMusic(AudioTag);
	}

	if (Row->PlaybackPolicy != EBGSystemAudioPlaybackPolicy::OneShot)
	{
		if (TObjectPtr<UAudioComponent>* Existing = ActiveSounds.Find(AudioTag))
		{
			if (IsValid(*Existing) && (*Existing)->IsPlaying())
			{
				return *Existing;
			}
			ActiveSounds.Remove(AudioTag);
		}
	}

	UAudioComponent* Component = CreateComponent(AudioTag, *Row);
	if (!Component)
	{
		return nullptr;
	}

	if (Row->PlaybackPolicy != EBGSystemAudioPlaybackPolicy::OneShot)
	{
		ActiveSounds.Add(AudioTag, Component);
		Component->OnAudioFinished.AddUniqueDynamic(
			this, &UBGAudioSubsystem::HandleTrackedSoundFinished);
	}

	if (Row->FadeInDuration > 0.f)
	{
		Component->FadeIn(Row->FadeInDuration, Row->VolumeMultiplier);
	}
	else
	{
		Component->Play();
	}

	return Component;
}

float UBGAudioSubsystem::ResolveFadeOut(const FBGSystemAudioRow* Row, float Override) const
{
	return Override >= 0.f ? Override : (Row ? Row->FadeOutDuration : 0.f);
}

void UBGAudioSubsystem::StopByTag(FGameplayTag AudioTag, float FadeOutOverride)
{
	TObjectPtr<UAudioComponent>* Found = ActiveSounds.Find(AudioTag);
	if (!Found)
	{
		return;
	}

	UAudioComponent* Component = Found->Get();
	ActiveSounds.Remove(AudioTag);
	if (!IsValid(Component))
	{
		return;
	}

	const float FadeOut = ResolveFadeOut(FindRow(AudioTag), FadeOutOverride);
	if (FadeOut > 0.f)
	{
		Component->FadeOut(FadeOut, 0.f);
	}
	else
	{
		Component->Stop();
	}
}

UAudioComponent* UBGAudioSubsystem::PlayMusic(FGameplayTag AudioTag, float CrossFadeOverride)
{
	const FBGSystemAudioRow* Row = FindRow(AudioTag);
	if (!Row || Row->Category != EBGSystemAudioCategory::Music)
	{
		return nullptr;
	}

	if (CurrentMusicTag == AudioTag && IsValid(CurrentMusic) && CurrentMusic->IsPlaying())
	{
		return CurrentMusic;
	}

	UAudioComponent* NewMusic = CreateComponent(AudioTag, *Row);
	if (!NewMusic)
	{
		return nullptr; // 새 곡이 없다고 재생 중인 곡까지 끊지는 않는다.
	}

	const float FadeDuration = CrossFadeOverride >= 0.f
		? CrossFadeOverride
		: Row->FadeInDuration;

	if (IsValid(CurrentMusic))
	{
		// 이전 곡의 종료 콜백이 새 곡을 잘못 재시작하지 않게 먼저 해제한다.
		CurrentMusic->OnAudioFinished.RemoveDynamic(
			this, &UBGAudioSubsystem::HandleCurrentMusicFinished);

		const FBGSystemAudioRow* OldRow = FindRow(CurrentMusicTag);
		const float OldFadeOut = CrossFadeOverride >= 0.f
			? CrossFadeOverride
			: (OldRow ? OldRow->FadeOutDuration : 0.f);

		if (OldFadeOut > 0.f)
		{
			CurrentMusic->FadeOut(OldFadeOut, 0.f);
		}
		else
		{
			CurrentMusic->Stop();
		}
	}

	CurrentMusic = NewMusic;
	CurrentMusicTag = AudioTag;
	NewMusic->OnAudioFinished.AddUniqueDynamic(
		this, &UBGAudioSubsystem::HandleCurrentMusicFinished);
	if (FadeDuration > 0.f)
	{
		NewMusic->FadeIn(FadeDuration, Row->VolumeMultiplier);
	}
	else
	{
		NewMusic->Play();
	}

	return NewMusic;
}

void UBGAudioSubsystem::StopMusic(float FadeOutOverride)
{
	if (!IsValid(CurrentMusic))
	{
		CurrentMusic = nullptr;
		CurrentMusicTag = FGameplayTag();
		return;
	}

	const float FadeOut = ResolveFadeOut(FindRow(CurrentMusicTag), FadeOutOverride);
	CurrentMusic->OnAudioFinished.RemoveDynamic(
		this, &UBGAudioSubsystem::HandleCurrentMusicFinished);
	if (FadeOut > 0.f)
	{
		CurrentMusic->FadeOut(FadeOut, 0.f);
	}
	else
	{
		CurrentMusic->Stop();
	}

	CurrentMusic = nullptr;
	CurrentMusicTag = FGameplayTag();
}

void UBGAudioSubsystem::BindDefaultButtonSounds(UButton* Button)
{
	if (!Button)
	{
		return;
	}

	// NativeConstruct가 다시 호출돼도 같은 버튼에 중복 구독하지 않는다.
	Button->OnHovered.AddUniqueDynamic(this, &UBGAudioSubsystem::HandleButtonHoverBegin);
	Button->OnUnhovered.AddUniqueDynamic(this, &UBGAudioSubsystem::HandleButtonHoverEnd);
	Button->OnPressed.AddUniqueDynamic(this, &UBGAudioSubsystem::HandleButtonPressBegin);
	Button->OnReleased.AddUniqueDynamic(this, &UBGAudioSubsystem::HandleButtonPressEnd);
}

void UBGAudioSubsystem::BindDefaultButtonsInWidget(UUserWidget* RootWidget)
{
	if (!RootWidget || !RootWidget->WidgetTree)
	{
		return;
	}

	TArray<UWidget*> Widgets;
	RootWidget->WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		BindDefaultButtonSounds(Cast<UButton>(Widget));
	}
}

void UBGAudioSubsystem::HandleButtonHoverBegin()
{
	Play(BG::Audio::UI::Button::HoverBegin.GetTag());
}

void UBGAudioSubsystem::HandleButtonHoverEnd()
{
	Play(BG::Audio::UI::Button::HoverEnd.GetTag());
}

void UBGAudioSubsystem::HandleButtonPressBegin()
{
	Play(BG::Audio::UI::Button::PressBegin.GetTag());
}

void UBGAudioSubsystem::HandleButtonPressEnd()
{
	Play(BG::Audio::UI::Button::PressEnd.GetTag());
}

void UBGAudioSubsystem::HandleCurrentMusicFinished()
{
	if (!IsValid(CurrentMusic))
	{
		return;
	}

	const FBGSystemAudioRow* Row = FindRow(CurrentMusicTag);
	if (Row && Row->PlaybackPolicy == EBGSystemAudioPlaybackPolicy::Loop)
	{
		CurrentMusic->Play(0.f);
		return;
	}

	CurrentMusic = nullptr;
	CurrentMusicTag = FGameplayTag();
}

void UBGAudioSubsystem::HandleTrackedSoundFinished()
{
	for (auto It = ActiveSounds.CreateIterator(); It; ++It)
	{
		UAudioComponent* Component = It.Value().Get();
		if (IsValid(Component) && Component->IsPlaying())
		{
			continue;
		}

		const FBGSystemAudioRow* Row = FindRow(It.Key());
		if (IsValid(Component) && Row
			&& Row->PlaybackPolicy == EBGSystemAudioPlaybackPolicy::Loop)
		{
			Component->Play(0.f);
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

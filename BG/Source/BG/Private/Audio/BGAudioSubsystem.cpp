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
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundMix.h"

DEFINE_LOG_CATEGORY_STATIC(LogBGAudio, Log, All);

namespace
{
	const TCHAR* AudioSection = TEXT("BGAudio");
}

UBGAudioSubsystem* UBGAudioSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UBGAudioSubsystem>() : nullptr;
}

void UBGAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadVolumes();
	const UBGDataSettings* Settings = GetDefault<UBGDataSettings>();
	MasterSoundClass = Settings ? Settings->MasterSoundClass.LoadSynchronous() : nullptr;
	MusicSoundClass = Settings ? Settings->MusicSoundClass.LoadSynchronous() : nullptr;
	SFXSoundClass = Settings ? Settings->SFXSoundClass.LoadSynchronous() : nullptr;
	MasterSoundMix = Settings ? Settings->MasterSoundMix.LoadSynchronous() : nullptr;
	MusicSoundMix = Settings ? Settings->MusicSoundMix.LoadSynchronous() : nullptr;
	SFXSoundMix = Settings ? Settings->SFXSoundMix.LoadSynchronous() : nullptr;
	if (!MasterSoundClass || !MusicSoundClass || !SFXSoundClass
		|| !MasterSoundMix || !MusicSoundMix || !SFXSoundMix)
	{
		UE_LOG(LogBGAudio, Warning,
			TEXT("볼륨 설정 에셋 로드 실패 — MasterClass=%s MusicClass=%s SFXClass=%s "
				"MasterMix=%s MusicMix=%s SFXMix=%s. BGDataSettings 에서 지정할 것."),
			Settings ? *Settings->MasterSoundClass.ToString() : TEXT("BGDataSettings 없음"),
			Settings ? *Settings->MusicSoundClass.ToString() : TEXT("BGDataSettings 없음"),
			Settings ? *Settings->SFXSoundClass.ToString() : TEXT("BGDataSettings 없음"),
			Settings ? *Settings->MasterSoundMix.ToString() : TEXT("BGDataSettings 없음"),
			Settings ? *Settings->MusicSoundMix.ToString() : TEXT("BGDataSettings 없음"),
			Settings ? *Settings->SFXSoundMix.ToString() : TEXT("BGDataSettings 없음"));
	}
	EnsureVolumesApplied();

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
	UWorld* World = GetWorld();
	if (World)
	{
		if (bMasterSoundMixPushed && MasterSoundMix)
		{
			UGameplayStatics::PopSoundMixModifier(World, MasterSoundMix);
		}
		if (bMusicSoundMixPushed && MusicSoundMix)
		{
			UGameplayStatics::PopSoundMixModifier(World, MusicSoundMix);
		}
		if (bSFXSoundMixPushed && SFXSoundMix)
		{
			UGameplayStatics::PopSoundMixModifier(World, SFXSoundMix);
		}
	}
	bMasterSoundMixPushed = false;
	bMusicSoundMixPushed = false;
	bSFXSoundMixPushed = false;
	MasterSoundMix = nullptr;
	MusicSoundMix = nullptr;
	SFXSoundMix = nullptr;
	MasterSoundClass = nullptr;
	MusicSoundClass = nullptr;
	SFXSoundClass = nullptr;

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

//설정에서 볼륨값 불러오기 함수
void UBGAudioSubsystem::LoadVolumes()
{
	//	저장된 값이 없으면 GetFloat 가 변수를 건드리지 않는다 — 선언부 기본값 1 이 그대로 남는다.
	GConfig->GetFloat(AudioSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(AudioSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
	GConfig->GetFloat(AudioSection, TEXT("SFXVolume"), SFXVolume, GGameUserSettingsIni);

	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	MusicVolume  = FMath::Clamp(MusicVolume, 0.f, 1.f);
	SFXVolume    = FMath::Clamp(SFXVolume, 0.f, 1.f);
}

//설정값에 볼륨값 저장 함수
void UBGAudioSubsystem::SaveVolumes() const
{
	GConfig->SetFloat(AudioSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
	GConfig->SetFloat(AudioSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
	GConfig->SetFloat(AudioSection, TEXT("SFXVolume"), SFXVolume, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

//볼륨 적용 여부 확인 함수
void UBGAudioSubsystem::EnsureVolumesApplied()
{
	UWorld* World = GetWorld();
	if (!World || !World->bAllowAudioPlayback || !World->GetAudioDeviceRaw())
	{
		return;
	}

	// SC_Basic의 자식으로 SC_Music과 SC_SFX가 연결되어 있어 엔진이
	// Master × Category 볼륨을 자연스럽게 합성한다.
	auto ApplyMix = [World](USoundMix* Mix, USoundClass* Class, float Volume,
		bool bApplyToChildren, bool& bPushed)
	{
		if (!Mix || !Class)
		{
			return;
		}

		UGameplayStatics::SetSoundMixClassOverride(
			World, Mix, Class, FMath::Clamp(Volume, 0.f, 1.f),
			/*Pitch=*/1.f, /*FadeInTime=*/0.05f, bApplyToChildren);

		if (!bPushed)
		{
			UGameplayStatics::PushSoundMixModifier(World, Mix);
			bPushed = true;
		}
	};

	ApplyMix(MasterSoundMix, MasterSoundClass, MasterVolume,
		/*bApplyToChildren=*/true, bMasterSoundMixPushed);
	ApplyMix(MusicSoundMix, MusicSoundClass, MusicVolume,
		/*bApplyToChildren=*/false, bMusicSoundMixPushed);
	ApplyMix(SFXSoundMix, SFXSoundClass, SFXVolume,
		/*bApplyToChildren=*/false, bSFXSoundMixPushed);
}

void UBGAudioSubsystem::ApplyVolumes()
{
	EnsureVolumesApplied();
}

//마스터 볼륨 설정 함수
void UBGAudioSubsystem::SetMasterVolume(float InVolume)
{
	MasterVolume = FMath::Clamp(InVolume, 0.f, 1.f);
	ApplyVolumes();
	SaveVolumes();
}

//BGM 볼륨 설정 함수
void UBGAudioSubsystem::SetMusicVolume(float InVolume)
{
	MusicVolume = FMath::Clamp(InVolume, 0.f, 1.f);
	ApplyVolumes();
	SaveVolumes();
}

//효과음 볼륨 설정 함수
void UBGAudioSubsystem::SetSFXVolume(float InVolume)
{
	SFXVolume = FMath::Clamp(InVolume, 0.f, 1.f);
	ApplyVolumes();
	SaveVolumes();
}

//Row 분류 → Sound Class
USoundClass* UBGAudioSubsystem::ResolveSoundClass(const FBGSystemAudioRow& Row) const
{
	//	UI 와 System 은 한 슬라이더로 묶는다 — 플레이어에게는 둘 다 "효과음" 이다.
	return Row.Category == EBGSystemAudioCategory::Music ? MusicSoundClass : SFXSoundClass;
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
	EnsureVolumesApplied();

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

	UAudioComponent* Component = UGameplayStatics::CreateSound2D(
		this,
		Sound,
		Row.VolumeMultiplier,
		Row.PitchMultiplier,
		/*StartTime=*/0.f,
		Concurrency,
		Row.bPersistAcrossLevelTransition,
		/*bAutoDestroy=*/Row.PlaybackPolicy == EBGSystemAudioPlaybackPolicy::OneShot);

	//	사운드 에셋마다 클래스를 지정해 두지 않아도 되도록 여기서 분류대로 보낸다 —
	//	DT 의 Category 만 맞으면 BGM · 효과음 슬라이더가 바로 먹는다.
	if (Component)
	{
		if (USoundClass* Class = ResolveSoundClass(Row))
		{
			Component->SoundClassOverride = Class;
		}
	}

	return Component;
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
	// 음악으로 분류된 태그도 같은 종료 경로에서 페이드아웃한다.
	if (CurrentMusicTag == AudioTag)
	{
		StopMusic(FadeOutOverride);
		return;
	}
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

void UBGAudioSubsystem::BindButtonSoundGroup(UButton* Button, EBGUIButtonSoundGroup Group)
{
	if (!Button)
	{
		return;
	}

	BindDefaultButtonSounds(Button);
	Button->OnPressed.RemoveDynamic(this, &UBGAudioSubsystem::HandleButtonPressBegin);
	Button->OnPressed.RemoveDynamic(this, &UBGAudioSubsystem::HandleConfirmButtonPress);
	Button->OnPressed.RemoveDynamic(this, &UBGAudioSubsystem::HandleNavigationButtonPress);
	Button->OnPressed.RemoveDynamic(this, &UBGAudioSubsystem::HandleSelectionButtonPress);
	Button->OnPressed.RemoveDynamic(this, &UBGAudioSubsystem::HandleExitButtonPress);
	switch (Group)
	{
	case EBGUIButtonSoundGroup::Confirm:
		Button->OnPressed.AddUniqueDynamic(this, &UBGAudioSubsystem::HandleConfirmButtonPress);
		break;
	case EBGUIButtonSoundGroup::Navigation:
		Button->OnPressed.AddUniqueDynamic(this, &UBGAudioSubsystem::HandleNavigationButtonPress);
		break;
	case EBGUIButtonSoundGroup::Selection:
		Button->OnPressed.AddUniqueDynamic(this, &UBGAudioSubsystem::HandleSelectionButtonPress);
		break;
	case EBGUIButtonSoundGroup::Exit:
		Button->OnPressed.AddUniqueDynamic(this, &UBGAudioSubsystem::HandleExitButtonPress);
		break;
	default:
		Button->OnPressed.AddUniqueDynamic(this, &UBGAudioSubsystem::HandleButtonPressBegin);
		break;
	}
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

void UBGAudioSubsystem::HandleConfirmButtonPress()
{
	Play(BG::Audio::UI::Button::Confirm.GetTag());
}

void UBGAudioSubsystem::HandleNavigationButtonPress()
{
	Play(BG::Audio::UI::Button::Navigation.GetTag());
}

void UBGAudioSubsystem::HandleSelectionButtonPress()
{
	Play(BG::Audio::UI::Button::Selection.GetTag());
}

void UBGAudioSubsystem::HandleExitButtonPress()
{
	Play(BG::Audio::UI::Button::Exit.GetTag());
}

void UBGAudioSubsystem::HandleButtonPressEnd()
{
	Play(BG::Audio::UI::Button::PressEnd.GetTag());
}

void UBGAudioSubsystem::HandleCurrentMusicFinished()
{
	// Loop 행의 같은 에셋을 다시 재생하고, 음원 선택은 연결된 Cue가 담당한다.
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

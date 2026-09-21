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
	MusicSoundClass = Settings ? Settings->MusicSoundClass.LoadSynchronous() : nullptr;
	SFXSoundClass = Settings ? Settings->SFXSoundClass.LoadSynchronous() : nullptr;
	UserSettingsSoundMix = Settings ? Settings->UserSettingsSoundMix.LoadSynchronous() : nullptr;
	//	비면 그 갈래는 슬라이더가 움직여도 소리가 안 변한다.
	if (!MusicSoundClass || !SFXSoundClass || !UserSettingsSoundMix)
	{
		UE_LOG(LogBGAudio, Warning,
			TEXT("볼륨 설정 에셋 로드 실패 — Music=%s SFX=%s SoundMix=%s. BGDataSettings 에서 지정할 것."),
			Settings ? *Settings->MusicSoundClass.ToString() : TEXT("BGDataSettings 없음"),
			Settings ? *Settings->SFXSoundClass.ToString() : TEXT("BGDataSettings 없음"),
			Settings ? *Settings->UserSettingsSoundMix.ToString() : TEXT("BGDataSettings 없음"));
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
	if (bUserSettingsSoundMixPushed && UserSettingsSoundMix && GetWorld())
	{
		UGameplayStatics::PopSoundMixModifier(this, UserSettingsSoundMix);
	}
	bUserSettingsSoundMixPushed = false;
	UserSettingsSoundMix = nullptr;
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
	if (!World || !World->bAllowAudioPlayback || !World->GetAudioDeviceRaw()
		|| !UserSettingsSoundMix)
	{
		return;
	}

	// GameInstanceSubsystem 초기화 시점에는 PIE 오디오 장치가 아직 없을 수 있다.
	// 오버라이드를 먼저 등록하고, 실제 장치가 준비된 뒤에만 Mix를 푸시 완료로 기록한다.
	//
	//	⚠️ 두 클래스는 서로 부모·자식이 아니다 — 마스터 곱셈을 여기서 직접 한다.
	//	   계층에 기대면 부모의 ChildClasses 가 저장되지 않았을 때 자식이 계층 순회에서 빠져
	//	   마스터가 조용히 안 먹는다. bApplyToChildren 이 false 인 것도 같은 이유다.
	auto Override = [this, World](USoundClass* Class, float Volume)
	{
		if (Class)
		{
			UGameplayStatics::SetSoundMixClassOverride(
				World, UserSettingsSoundMix, Class, FMath::Clamp(Volume, 0.f, 1.f),
				/*Pitch=*/1.f, /*FadeInTime=*/0.05f, /*bApplyToChildren=*/false);
		}
	};

	Override(MusicSoundClass, MasterVolume * MusicVolume);
	Override(SFXSoundClass, MasterVolume * SFXVolume);

	if (!bUserSettingsSoundMixPushed)
	{
		UGameplayStatics::PushSoundMixModifier(World, UserSettingsSoundMix);
		bUserSettingsSoundMixPushed = true;
		UE_LOG(LogBGAudio, Log,
			TEXT("사용자 볼륨 Sound Mix 활성화 (마스터=%.2f BGM=%.2f 효과음=%.2f)"),
			MasterVolume, MusicVolume, SFXVolume);
	}
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

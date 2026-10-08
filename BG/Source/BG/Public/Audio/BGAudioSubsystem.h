#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BGAudioSubsystem.generated.h"

class UAudioComponent;
class UButton;
class UDataTable;
class USoundClass;
class USoundMix;
class UUserWidget;
struct FBGSystemAudioRow;

/** 버튼 클릭의 의미에 따라 누름 소리만 구분한다. 호버/뗌 소리는 공통이다. */
UENUM(BlueprintType)
enum class EBGUIButtonSoundGroup : uint8
{
	Default,
	Confirm,
	Navigation,
	Selection,
	Exit
};

/**
 * UI·시스템·BGM 전용 2D 오디오 관리자.
 * 위치 기반 효과음은 Niagara, 캐릭터 동작음은 Montage가 담당한다.
 */
UCLASS()
class BG_API UBGAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** World/GameInstance/UserWidget 어느 문맥에서도 같은 인스턴스를 찾는 짧은 진입점. */
	static UBGAudioSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Row 정책(OneShot/Singleton/Loop/Music)에 따라 2D 사운드를 재생한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Audio", meta = (Categories = "Audio"))
	UAudioComponent* Play(FGameplayTag AudioTag);

	/** 태그로 추적 중인 Singleton/Loop 사운드를 Row 기본 FadeOut으로 중단한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Audio", meta = (Categories = "Audio"))
	void StopByTag(FGameplayTag AudioTag, float FadeOutOverride = -1.f);

	/** 기존 BGM과 새 BGM을 겹쳐 크로스페이드한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Audio", meta = (Categories = "Audio"))
	UAudioComponent* PlayMusic(FGameplayTag AudioTag, float CrossFadeOverride = -1.f);

	UFUNCTION(BlueprintCallable, Category = "BG|Audio")
	void StopMusic(float FadeOutOverride = -1.f);

	/** 기존 UButton 교체 없이 Hover 시작/종료, Press 시작/종료 네 종류를 연결한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Audio")
	void BindDefaultButtonSounds(UButton* Button);

	/** 기존 공통 바인딩의 누름 소리를 역할별 소리로 교체한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Audio")
	void BindButtonSoundGroup(UButton* Button, EBGUIButtonSoundGroup Group);

	/** 현재 WidgetTree에 들어 있는 모든 기존 UButton을 교체 없이 연결한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Audio")
	void BindDefaultButtonsInWidget(UUserWidget* RootWidget);

	UFUNCTION(BlueprintPure, Category = "BG|Audio")
	bool HasAudioRow(FGameplayTag AudioTag) const;

	//	SC_Basic 아래에 SC_Music · SC_SFX를 두므로 마스터와 카테고리 볼륨이 엔진에서 곱해진다.
	//	분류는 사운드 에셋의 SoundClassObject가 정하며, 서브시스템 재생은 Row.Category로 보정한다.

	/** SC_Basic과 모든 자식 클래스에 적용되는 사용자 마스터 볼륨. */
	UFUNCTION(BlueprintPure, Category = "BG|Audio")
	float GetMasterVolume() const { return MasterVolume; }

	/** SC_Music에 적용되는 BGM 볼륨. */
	UFUNCTION(BlueprintPure, Category = "BG|Audio")
	float GetMusicVolume() const { return MusicVolume; }

	/** SC_SFX에 적용되는 효과음 볼륨. DT의 UI · System 행이 모두 여기로 간다. */
	UFUNCTION(BlueprintPure, Category = "BG|Audio")
	float GetSFXVolume() const { return SFXVolume; }

	/** 값을 저장하고 Sound Mix 오버라이드에 즉시 반영한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Audio")
	void SetMasterVolume(float InVolume);

	UFUNCTION(BlueprintCallable, Category = "BG|Audio")
	void SetMusicVolume(float InVolume);

	UFUNCTION(BlueprintCallable, Category = "BG|Audio")
	void SetSFXVolume(float InVolume);

	/** 저장값을 바꾸지 않고 현재 World의 오디오 장치에 세 볼륨을 다시 적용한다. */
	void ApplyVolumes();

private:
	void LoadVolumes();
	void SaveVolumes() const;
	void EnsureVolumesApplied();

	/** Row 분류에 맞는 Sound Class. 못 찾으면 nullptr — 그때는 사운드 에셋이 가진 클래스를 그대로 쓴다. */
	USoundClass* ResolveSoundClass(const FBGSystemAudioRow& Row) const;

	const FBGSystemAudioRow* FindRow(FGameplayTag AudioTag) const;
	UAudioComponent* CreateComponent(FGameplayTag AudioTag, const FBGSystemAudioRow& Row);
	float ResolveFadeOut(const FBGSystemAudioRow* Row, float Override) const;

	UFUNCTION()
	void HandleButtonHoverBegin();

	UFUNCTION()
	void HandleButtonHoverEnd();

	UFUNCTION()
	void HandleButtonPressBegin();

	UFUNCTION()
	void HandleConfirmButtonPress();

	UFUNCTION()
	void HandleNavigationButtonPress();

	UFUNCTION()
	void HandleSelectionButtonPress();

	UFUNCTION()
	void HandleExitButtonPress();

	UFUNCTION()
	void HandleButtonPressEnd();

	/** SoundWave 자체가 Loop가 아니어도 DT의 Loop 정책을 실제 반복으로 보장한다. */
	UFUNCTION()
	void HandleCurrentMusicFinished();

	/** Singleton 정리와 비-BGM Loop 재시작을 한곳에서 처리한다. */
	UFUNCTION()
	void HandleTrackedSoundFinished();

	UPROPERTY()
	TObjectPtr<UDataTable> AudioTable;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> SFXSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> MasterSoundMix;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> MusicSoundMix;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> SFXSoundMix;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UAudioComponent>> ActiveSounds;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CurrentMusic;

	FGameplayTag CurrentMusicTag;

	float MasterVolume = 1.f;
	float MusicVolume  = 1.f;
	float SFXVolume    = 1.f;
	bool bMasterSoundMixPushed = false;
	bool bMusicSoundMixPushed = false;
	bool bSFXSoundMixPushed = false;
};

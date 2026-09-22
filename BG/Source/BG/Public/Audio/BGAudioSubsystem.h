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

	/** 현재 WidgetTree에 들어 있는 모든 기존 UButton을 교체 없이 연결한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Audio")
	void BindDefaultButtonsInWidget(UUserWidget* RootWidget);

	UFUNCTION(BlueprintPure, Category = "BG|Audio")
	bool HasAudioRow(FGameplayTag AudioTag) const;

	//	볼륨은 마스터 · BGM · 효과음 셋이지만 Sound Class 는 둘뿐이다(SC_Music · SC_Basic).
	//	마스터는 클래스가 아니라 두 값에 곱해지는 배수이고, 곱셈은 EnsureVolumesApplied 가 직접 한다.
	//
	//	⚠️ 둘은 서로 부모·자식이 아니다. 계층에 기대면 부모의 ChildClasses 가 저장되지 않았을 때
	//	   자식이 계층 순회에서 빠져 마스터가 조용히 안 먹는다.
	//
	//	분류는 사운드 에셋의 SoundClassObject 가 정한다 — Niagara 처럼 서브시스템을 안 거치는
	//	재생 경로도 그 값을 따르기 때문이다. 서브시스템으로 낸 소리는 CreateComponent 가
	//	Row.Category 로 한 번 더 덮어써서 DT 쪽 분류가 이기게 한다.

	/** 사용자 마스터 볼륨. 아래 둘에 곱해져서 적용된다. */
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
	TObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> SFXSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> UserSettingsSoundMix;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UAudioComponent>> ActiveSounds;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CurrentMusic;

	FGameplayTag CurrentMusicTag;

	float MasterVolume = 1.f;
	float MusicVolume  = 1.f;
	float SFXVolume    = 1.f;
	bool bUserSettingsSoundMixPushed = false;
};

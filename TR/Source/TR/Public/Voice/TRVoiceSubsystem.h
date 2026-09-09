// TRVoiceSubsystem.h
// 음성 설정의 단일 창구 — 저장, voice.* CVar 반영, 무전 컴포넌트 부착, 임시 패널 토글.

#pragma once

#include "CoreMinimal.h"
#include "Online/CoreOnline.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "TRVoiceSubsystem.generated.h"

class APlayerController;
class IConsoleObject;
class ULocalPlayer;
class USoundAttenuation;
class UTRVoiceComponent;
class UUserWidget;

/** 값이 바뀌었다. 패널이 슬라이더를 다시 채우는 데 쓴다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTRVoiceSettingsChanged);

/** 발화 시작/종료 알림. 블루프린트에서 구독할 수 있다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTRVoiceTalkingChanged, const FString&, PlayerId, bool, bIsTalking);

/** 접속자 목록 한 줄. 패널이 개인별 음량 행을 만드는 데 쓴다. */
USTRUCT(BlueprintType)
struct FTRVoicePlayerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TR|Voice")
	FString NetId;

	UPROPERTY(BlueprintReadOnly, Category = "TR|Voice")
	FString DisplayName;

	// 0~2. 1 이 원음.
	UPROPERTY(BlueprintReadOnly, Category = "TR|Voice")
	float Volume = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "TR|Voice")
	bool bIsTalking = false;

	// 0~1. 지원 안 되면 0.
	UPROPERTY(BlueprintReadOnly, Category = "TR|Voice")
	float Level = 0.f;
};

/**
 * 설정값은 GameUserSettings.ini 의 [/Script/TR.TRVoiceSubsystem] 에 저장하고,
 * 런타임에는 voice.* 콘솔 변수로 반영한다.
 */
UCLASS(Config = GameUserSettings)
class TR_API UTRVoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 월드 컨텍스트의 GameInstance 에서 얻는다. */
	static UTRVoiceSubsystem* Get(const UObject* WorldContext);

	//--- 마이크 설정 ---
	UFUNCTION(BlueprintPure, Category = "TR|Voice|Mic")
	bool IsPushToTalk() const { return bPushToTalk; }

	UFUNCTION(BlueprintPure, Category = "TR|Voice|Mic")
	float GetMicGain() const { return MicGain; }

	UFUNCTION(BlueprintPure, Category = "TR|Voice|Mic")
	float GetMicThreshold() const { return MicThreshold; }

	UFUNCTION(BlueprintPure, Category = "TR|Voice|Mic")
	float GetMicNoiseGate() const { return MicNoiseGate; }

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Mic")
	void SetPushToTalk(bool bValue);

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Mic")
	void SetMicGain(float Value);

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Mic")
	void SetMicThreshold(float Value);

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Mic")
	void SetMicNoiseGate(float Value);

	//--- 스피커 설정 ---
	UFUNCTION(BlueprintPure, Category = "TR|Voice|Output")
	bool IsOutputMuted() const { return bOutputMuted; }

	UFUNCTION(BlueprintPure, Category = "TR|Voice|Output")
	float GetJitterDelay() const { return JitterDelay; }

	/** 수신 음성 전체 음량 0~2. 개인 음량과 곱해진다. */
	UFUNCTION(BlueprintPure, Category = "TR|Voice|Output")
	float GetOutputVolume() const { return OutputVolume; }

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Output")
	void SetOutputMuted(bool bValue);

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Output")
	void SetJitterDelay(float Value);

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Output")
	void SetOutputVolume(float Value);

	//--- 공간 음성 ---
	/** 켜면 상대 음성이 그 플레이어 위치에서 나고 거리에 따라 작아진다. */
	UFUNCTION(BlueprintPure, Category = "TR|Voice|Spatial")
	bool IsSpatialVoice() const { return bSpatialVoice; }

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Spatial")
	void SetSpatialVoice(bool bValue);

	/** 감쇠 시작 거리(cm). 이 안에서는 원음. */
	UFUNCTION(BlueprintPure, Category = "TR|Voice|Spatial")
	float GetVoiceInnerRadius() const { return VoiceInnerRadius; }

	/** 감쇠 끝 거리(cm). 안쪽 반지름에서 이만큼 더 가면 들리지 않는다. */
	UFUNCTION(BlueprintPure, Category = "TR|Voice|Spatial")
	float GetVoiceFalloffDistance() const { return VoiceFalloffDistance; }

	//--- 개인별 음량 ---
	UFUNCTION(BlueprintPure, Category = "TR|Voice|Players")
	float GetPlayerVolume(const FString& NetId) const;

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Players")
	void SetPlayerVolume(const FString& NetId, float Value);

	/** 나를 뺀 접속자. 토커가 없는 사람에게는 이 호출에서 붙인다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Players")
	TArray<FTRVoicePlayerInfo> GetRemotePlayers();

	//--- 조회 ---
	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	bool IsLocalTalking() const;

	/** 내 마이크 입력 크기 0~1. 캡처 중이 아니면 0. */
	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	float GetLocalMicLevel() const;

	/** 송신 전에 로컬 토커를 등록한다. 여러 번 불러도 된다. */
	bool EnsureLocalTalkerRegistered();

	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	bool IsVoiceInterfaceReady() const;

	/** 진단 문자열. bIncludeEngineDump 면 엔진 보이스 상태 덤프를 뒤에 붙인다. */
	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	FString GetStatusText(bool bIncludeEngineDump = true) const;

	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	UTRVoiceComponent* GetLocalVoiceComponent() const;

	//--- 적용 / 저장 ---
	/** 저장값을 CVar 로 밀어 넣는다. 값 변경 때마다 자동으로 불린다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Voice")
	void ApplyToRuntime();

	UFUNCTION(BlueprintCallable, Category = "TR|Voice")
	void SaveVoiceSettings();

	UFUNCTION(BlueprintCallable, Category = "TR|Voice")
	void ResetVoiceSettings();

	//--- 임시 설정 패널 ---
	UFUNCTION(BlueprintCallable, Category = "TR|Voice|UI")
	void ToggleSettingsUI();

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|UI")
	void SetSettingsUIVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "TR|Voice|UI")
	bool IsSettingsUIVisible() const;

	UPROPERTY(BlueprintAssignable, Category = "TR|Voice")
	FTRVoiceSettingsChanged OnVoiceSettingsChanged;

	UPROPERTY(BlueprintAssignable, Category = "TR|Voice")
	FTRVoiceTalkingChanged OnTalkingChanged;

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** 로컬 플레이어가 생기면 컨트롤러 변경을 구독하고 컴포넌트를 붙인다. */
	void HandleLocalPlayerAdded(ULocalPlayer* LocalPlayer);

	/** 보이스 인터페이스 델리게이트를 한 번만 구독하고 로컬 토커를 등록한다. */
	void BindVoiceInterfaceDelegates();

	void UnbindVoiceInterfaceDelegates();

	void HandleTalkingStateChanged(FUniqueNetIdRef PlayerId, bool bIsTalking);

	void HandlePlayerControllerChanged(APlayerController* NewPC);

	/** 로컬 컨트롤러에 컴포넌트가 없으면 붙이고, 있으면 키 바인딩만 다시 시도한다. */
	void EnsureVoiceComponent(APlayerController* PC);

	void RegisterConsoleCommands();

	void UnregisterConsoleCommands();

	APlayerController* GetLocalController() const;

	/** 해당 PlayerState 에 토커가 없으면 붙인다. UniqueId 가 아직 없으면 다음 기회로. */
	class UTRVoiceTalker* EnsureTalker(class APlayerState* PlayerState);

	void ApplyAllPlayerVolumes();

	/** 감쇠 설정 객체. 없으면 ini 값으로 만든다. */
	USoundAttenuation* GetVoiceAttenuation();

	void ApplySpatialToTalker(class UTRVoiceTalker* Talker);

	float GetEffectiveVolume(const FString& NetId) const;

	uint32 GetLocalUserNum() const;

	//--- GameUserSettings.ini 저장값 ---
	UPROPERTY(Config)
	bool bSpatialVoice = true;

	UPROPERTY(Config)
	float OutputVolume = 1.f;

	// NetId 문자열 -> 개인 음량.
	UPROPERTY(Config)
	TMap<FString, float> PlayerVolumes;

	UPROPERTY(Config)
	bool bPushToTalk = true;

	UPROPERTY(Config)
	float MicGain = 1.f;

	UPROPERTY(Config)
	float MicThreshold = 0.08f;

	UPROPERTY(Config)
	float MicNoiseGate = 0.f;

	UPROPERTY(Config)
	bool bOutputMuted = false;

	UPROPERTY(Config)
	float JitterDelay = 0.2f;

	/** 임시 설정 패널 WBP. DefaultGame.ini 의 [/Script/TR.TRVoiceSubsystem] 에서 읽는다. */
	FSoftClassPath SettingsWidgetClass;

	//--- DefaultGame.ini 프로젝트값 ---
	float VoiceInnerRadius = 300.f;

	float VoiceFalloffDistance = 2500.f;

	// 지정하면 ini 값 대신 이 감쇠 애셋을 쓴다.
	FSoftObjectPath VoiceAttenuationAsset;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> VoiceAttenuation;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SettingsWidget;

	// 패널을 열기 전 커서 상태. 닫을 때 그대로 되돌린다.
	bool bCursorWasVisible = false;

	TArray<IConsoleObject*> ConsoleCommands;

	FDelegateHandle PostLoadMapHandle;

	FDelegateHandle TalkingStateHandle;

	TWeakObjectPtr<ULocalPlayer> BoundLocalPlayer;
};

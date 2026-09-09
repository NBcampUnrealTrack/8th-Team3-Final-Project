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
class UTRVoiceComponent;
class UUserWidget;

/** 값이 바뀌었다. 패널이 슬라이더를 다시 채우는 데 쓴다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTRVoiceSettingsChanged);

/**
 * 누군가 말하기 시작했거나 멈췄다. 엔진의 OnPlayerTalkingStateChanged 를
 * 블루프린트가 붙을 수 있는 형태로 옮긴 것이다.
 */
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
 * 설정값은 GameUserSettings.ini 의 [/Script/TR.TRVoiceSubsystem] 에 남는다.
 * 실제 반영은 전부 voice.* 콘솔 변수로 나간다 — 엔진이 그 경로만 열어 두었다.
 */
UCLASS(Config = GameUserSettings)
class TR_API UTRVoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 월드 컨텍스트로 얻는다. PIE 는 인스턴스마다 별도 서브시스템이라 이 경로를 써야 한다. */
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

	/** 내 마이크 입력 크기 0~1. 캡처가 멈춰 있으면(무전 키를 안 누른 상태) 0. */
	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	float GetLocalMicLevel() const;

	/**
	 * 송신 전에 로컬 토커를 엔진에 등록한다. 등록이 없으면 StartLocalVoiceProcessing 이
	 * 조용히 실패한다(0xFFFFFFFF). 여러 번 불러도 된다.
	 */
	bool EnsureLocalTalkerRegistered();

	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	bool IsVoiceInterfaceReady() const;

	/** 진단 문자열. 엔진 덤프는 길어서 패널에서는 뺀다. 콘솔 TR.Voice.Status 는 포함한다. */
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

	/** 로컬 플레이어가 생기는 시점. PIE 는 맵을 로드하지 않아 이 경로로만 잡힌다. */
	void HandleLocalPlayerAdded(ULocalPlayer* LocalPlayer);

	/** 보이스 인터페이스는 늦게 생긴다. 생긴 뒤 한 번만 붙는다. */
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

	float GetEffectiveVolume(const FString& NetId) const;

	uint32 GetLocalUserNum() const;

	//--- GameUserSettings.ini 저장값 ---
	UPROPERTY(Config)
	float OutputVolume = 1.f;

	// NetId 문자열 -> 개인 음량. 스팀은 ID 가 고정이라 다음 판에도 유지된다.
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

	/**
	 * 임시 설정 패널 WBP. 사용자 설정이 아니라 프로젝트 설정이라 DefaultGame.ini 에 둔다.
	 * 이 클래스의 Config 파일은 GameUserSettings.ini 이므로 UPROPERTY(Config) 로는 못 읽는다.
	 * Initialize 에서 GGameIni 를 직접 조회한다.
	 */
	FSoftClassPath SettingsWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SettingsWidget;

	// 패널을 열기 전 커서 상태. 닫을 때 그대로 되돌린다.
	bool bCursorWasVisible = false;

	TArray<IConsoleObject*> ConsoleCommands;

	FDelegateHandle PostLoadMapHandle;

	FDelegateHandle TalkingStateHandle;

	TWeakObjectPtr<ULocalPlayer> BoundLocalPlayer;
};

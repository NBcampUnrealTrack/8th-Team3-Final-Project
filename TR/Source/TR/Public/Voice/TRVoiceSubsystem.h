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

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Output")
	void SetOutputMuted(bool bValue);

	UFUNCTION(BlueprintCallable, Category = "TR|Voice|Output")
	void SetJitterDelay(float Value);

	//--- 조회 ---
	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	bool IsLocalTalking() const;

	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	bool IsVoiceInterfaceReady() const;

	/** 진단 문자열. 패널과 콘솔 명령이 같은 것을 쓴다. */
	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	FString GetStatusText() const;

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

	//--- GameUserSettings.ini 저장값 ---
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

	TArray<IConsoleObject*> ConsoleCommands;

	FDelegateHandle PostLoadMapHandle;

	FDelegateHandle TalkingStateHandle;

	TWeakObjectPtr<ULocalPlayer> BoundLocalPlayer;
};

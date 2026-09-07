// TRVoiceSubsystem.cpp

#include "Voice/TRVoiceSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/VoiceInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Session/TRNetLog.h"
#include "UObject/UObjectGlobals.h"
#include "Voice/TRVoiceComponent.h"

namespace TRVoiceCVar
{
	// 엔진이 노출한 유일한 조절 경로다. ini 키는 [Voice] bEnabled 말고는 없다.
	static const TCHAR* MicGain = TEXT("voice.MicInputGain");
	static const TCHAR* SilenceThreshold = TEXT("voice.SilenceDetectionThreshold");
	static const TCHAR* NoiseGate = TEXT("voice.MicNoiseGateThreshold");
	static const TCHAR* JitterDelay = TEXT("voice.JitterBufferDelay");
	static const TCHAR* MuteOutput = TEXT("voice.MuteAudioEngineOutput");

	static void SetFloat(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByGameSetting);
		}
	}

	static void SetInt(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByGameSetting);
		}
	}
}

UTRVoiceSubsystem* UTRVoiceSubsystem::Get(const UObject* WorldContext)
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext);
	return GI ? GI->GetSubsystem<UTRVoiceSubsystem>() : nullptr;
}

void UTRVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadConfig();

	// LoadConfig 는 GameUserSettings.ini 만 본다. 패널 경로는 프로젝트 설정이라 거기 없다.
	FString WidgetPath;
	if (GConfig->GetString(TEXT("/Script/TR.TRVoiceSubsystem"), TEXT("SettingsWidgetClass"), WidgetPath, GGameIni)
		&& !WidgetPath.IsEmpty())
	{
		SettingsWidgetClass = FSoftClassPath(WidgetPath);
	}

	ApplyToRuntime();

	// PostLoadMapWithWorld 는 UEngine::LoadMap 에서만 나온다. PIE 는 월드를 복제할 뿐이라
	// 그 경로로는 안 잡힌다. 로컬 플레이어 생성 이벤트가 양쪽 모두에서 확실하다.
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->OnLocalPlayerAddedEvent.AddUObject(this, &UTRVoiceSubsystem::HandleLocalPlayerAdded);
		HandleLocalPlayerAdded(GI->GetFirstGamePlayer());
	}

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UTRVoiceSubsystem::HandlePostLoadMap);

	RegisterConsoleCommands();
}

void UTRVoiceSubsystem::Deinitialize()
{
	UnregisterConsoleCommands();
	UnbindVoiceInterfaceDelegates();

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		GI->OnLocalPlayerAddedEvent.RemoveAll(this);
	}

	if (ULocalPlayer* LP = BoundLocalPlayer.Get())
	{
		LP->OnPlayerControllerChanged().RemoveAll(this);
	}
	BoundLocalPlayer.Reset();

	SetSettingsUIVisible(false);

	Super::Deinitialize();
}

void UTRVoiceSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld == nullptr || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	HandleLocalPlayerAdded(GetGameInstance()->GetFirstGamePlayer());
	BindVoiceInterfaceDelegates();
	ApplyToRuntime();
}

void UTRVoiceSubsystem::HandleLocalPlayerAdded(ULocalPlayer* LocalPlayer)
{
	if (LocalPlayer == nullptr)
	{
		return;
	}

	if (BoundLocalPlayer.Get() != LocalPlayer)
	{
		if (ULocalPlayer* Old = BoundLocalPlayer.Get())
		{
			Old->OnPlayerControllerChanged().RemoveAll(this);
		}

		LocalPlayer->OnPlayerControllerChanged().AddUObject(this, &UTRVoiceSubsystem::HandlePlayerControllerChanged);
		BoundLocalPlayer = LocalPlayer;
	}

	EnsureVoiceComponent(LocalPlayer->PlayerController);
}

void UTRVoiceSubsystem::BindVoiceInterfaceDelegates()
{
	if (TalkingStateHandle.IsValid())
	{
		return;
	}

	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	const IOnlineVoicePtr Voice = OSS ? OSS->GetVoiceInterface() : nullptr;
	if (!Voice.IsValid())
	{
		return;
	}

	TalkingStateHandle = Voice->AddOnPlayerTalkingStateChangedDelegate_Handle(
		FOnPlayerTalkingStateChangedDelegate::CreateUObject(this, &UTRVoiceSubsystem::HandleTalkingStateChanged));

	// 로컬 토커의 발화 알림과 IsLocalPlayerTalking 은 등록된 토커에만 나온다.
	// 엔진은 이 등록을 자동으로 하지 않는다(RegisterLocalTalkers 호출부가 없다).
	const ULocalPlayer* LP = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	Voice->RegisterLocalTalker(LP ? (uint32)LP->GetControllerId() : 0);
}

void UTRVoiceSubsystem::UnbindVoiceInterfaceDelegates()
{
	if (!TalkingStateHandle.IsValid())
	{
		return;
	}

	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (const IOnlineVoicePtr Voice = OSS ? OSS->GetVoiceInterface() : nullptr)
	{
		Voice->ClearOnPlayerTalkingStateChangedDelegate_Handle(TalkingStateHandle);

		const ULocalPlayer* LP = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
		Voice->UnregisterLocalTalker(LP ? (uint32)LP->GetControllerId() : 0);
	}
	TalkingStateHandle.Reset();
}

void UTRVoiceSubsystem::HandleTalkingStateChanged(FUniqueNetIdRef PlayerId, bool bIsTalking)
{
	// 상대 PC 에서 이 줄이 찍히면 음성 패킷이 실제로 도착한 것이다. 귀로 듣지 않아도 판정된다.
	UE_LOG(LogTRNet, Log, TEXT("발화 %s: %s"), bIsTalking ? TEXT("시작") : TEXT("종료"), *PlayerId->ToString());

	OnTalkingChanged.Broadcast(PlayerId->ToString(), bIsTalking);
}

void UTRVoiceSubsystem::HandlePlayerControllerChanged(APlayerController* NewPC)
{
	EnsureVoiceComponent(NewPC);
}

void UTRVoiceSubsystem::EnsureVoiceComponent(APlayerController* PC)
{
	if (PC == nullptr || !PC->IsLocalController())
	{
		return;
	}

	UTRVoiceComponent* Component = PC->FindComponentByClass<UTRVoiceComponent>();
	if (Component == nullptr)
	{
		Component = NewObject<UTRVoiceComponent>(PC, UTRVoiceComponent::StaticClass(), TEXT("TRVoiceComponent"));
		Component->RegisterComponent();
	}

	// 컴포넌트가 등록될 때 입력 컴포넌트가 아직 없을 수 있다. 여기서 한 번 더 시도한다.
	const bool bBound = Component->BindKeys();
	Component->ApplyTransmitMode(bPushToTalk);

	UE_LOG(LogTRNet, Log, TEXT("무전 컴포넌트 부착 (%s) — 키 연결=%s"),
		*PC->GetName(), bBound ? TEXT("완료") : TEXT("보류(입력 컴포넌트 없음)"));
}

APlayerController* UTRVoiceSubsystem::GetLocalController() const
{
	const UGameInstance* GI = GetGameInstance();
	const ULocalPlayer* LP = GI ? GI->GetFirstGamePlayer() : nullptr;
	return LP ? LP->PlayerController : nullptr;
}

UTRVoiceComponent* UTRVoiceSubsystem::GetLocalVoiceComponent() const
{
	APlayerController* PC = GetLocalController();
	return PC ? PC->FindComponentByClass<UTRVoiceComponent>() : nullptr;
}

//--- 설정 변경 ---

void UTRVoiceSubsystem::SetPushToTalk(bool bValue)
{
	bPushToTalk = bValue;

	if (UTRVoiceComponent* Component = GetLocalVoiceComponent())
	{
		Component->ApplyTransmitMode(bPushToTalk);
	}

	OnVoiceSettingsChanged.Broadcast();
}

void UTRVoiceSubsystem::SetMicGain(float Value)
{
	MicGain = FMath::Clamp(Value, 0.f, 4.f);
	ApplyToRuntime();
}

void UTRVoiceSubsystem::SetMicThreshold(float Value)
{
	MicThreshold = FMath::Clamp(Value, 0.f, 1.f);
	ApplyToRuntime();
}

void UTRVoiceSubsystem::SetMicNoiseGate(float Value)
{
	MicNoiseGate = FMath::Clamp(Value, 0.f, 1.f);
	ApplyToRuntime();
}

void UTRVoiceSubsystem::SetOutputMuted(bool bValue)
{
	bOutputMuted = bValue;
	ApplyToRuntime();
}

void UTRVoiceSubsystem::SetJitterDelay(float Value)
{
	JitterDelay = FMath::Clamp(Value, 0.f, 2.f);
	ApplyToRuntime();
}

void UTRVoiceSubsystem::ApplyToRuntime()
{
	TRVoiceCVar::SetFloat(TRVoiceCVar::MicGain, MicGain);
	TRVoiceCVar::SetFloat(TRVoiceCVar::SilenceThreshold, MicThreshold);
	TRVoiceCVar::SetFloat(TRVoiceCVar::NoiseGate, MicNoiseGate);
	TRVoiceCVar::SetFloat(TRVoiceCVar::JitterDelay, JitterDelay);
	TRVoiceCVar::SetInt(TRVoiceCVar::MuteOutput, bOutputMuted ? 1 : 0);

	OnVoiceSettingsChanged.Broadcast();
}

void UTRVoiceSubsystem::SaveVoiceSettings()
{
	SaveConfig();
	UE_LOG(LogTRNet, Log, TEXT("음성 설정 저장 — %s"), *GetStatusText());
}

void UTRVoiceSubsystem::ResetVoiceSettings()
{
	const UTRVoiceSubsystem* Defaults = GetDefault<UTRVoiceSubsystem>();
	bPushToTalk = Defaults->bPushToTalk;
	MicGain = Defaults->MicGain;
	MicThreshold = Defaults->MicThreshold;
	MicNoiseGate = Defaults->MicNoiseGate;
	bOutputMuted = Defaults->bOutputMuted;
	JitterDelay = Defaults->JitterDelay;

	if (UTRVoiceComponent* Component = GetLocalVoiceComponent())
	{
		Component->ApplyTransmitMode(bPushToTalk);
	}

	ApplyToRuntime();
}

//--- 조회 ---

bool UTRVoiceSubsystem::IsVoiceInterfaceReady() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	return OSS != nullptr && OSS->GetVoiceInterface().IsValid();
}

bool UTRVoiceSubsystem::IsLocalTalking() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	const IOnlineVoicePtr Voice = OSS ? OSS->GetVoiceInterface() : nullptr;
	if (!Voice.IsValid())
	{
		return false;
	}

	const ULocalPlayer* LP = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	return Voice->IsLocalPlayerTalking(LP ? (uint32)LP->GetControllerId() : 0);
}

FString UTRVoiceSubsystem::GetStatusText() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	const IOnlineVoicePtr Voice = OSS ? OSS->GetVoiceInterface() : nullptr;
	const UTRVoiceComponent* Component = GetLocalVoiceComponent();

	FString Text = FString::Printf(
		TEXT("OSS=%s 인터페이스=%s 무전키=%s 바인딩=%s\n")
		TEXT("송신모드=%s 송신중=%s 증폭=%.2f 문턱=%.3f 노이즈게이트=%.3f\n")
		TEXT("출력음소거=%s 지터=%.2fs"),
		OSS ? *OSS->GetSubsystemName().ToString() : TEXT("없음"),
		Voice.IsValid() ? TEXT("있음") : TEXT("없음"),
		Component ? *Component->GetPushToTalkKey().ToString() : TEXT("컴포넌트없음"),
		(Component && Component->IsBound()) ? TEXT("완료") : TEXT("미완료"),
		bPushToTalk ? TEXT("무전") : TEXT("오픈마이크"),
		IsLocalTalking() ? TEXT("예") : TEXT("아니오"),
		MicGain, MicThreshold, MicNoiseGate,
		bOutputMuted ? TEXT("켜짐") : TEXT("꺼짐"),
		JitterDelay);

	if (Voice.IsValid())
	{
		Text += TEXT("\n") + Voice->GetVoiceDebugState();
	}

	return Text;
}

//--- 임시 설정 패널 ---

bool UTRVoiceSubsystem::IsSettingsUIVisible() const
{
	return SettingsWidget != nullptr && SettingsWidget->IsInViewport();
}

void UTRVoiceSubsystem::ToggleSettingsUI()
{
	SetSettingsUIVisible(!IsSettingsUIVisible());
}

void UTRVoiceSubsystem::SetSettingsUIVisible(bool bVisible)
{
	if (!bVisible)
	{
		if (SettingsWidget != nullptr)
		{
			SettingsWidget->RemoveFromParent();
			SettingsWidget = nullptr;
		}
		return;
	}

	if (SettingsWidget != nullptr)
	{
		return;
	}

	APlayerController* PC = GetLocalController();
	if (PC == nullptr)
	{
		UE_LOG(LogTRNet, Warning, TEXT("음성 설정 패널: 로컬 컨트롤러가 없어 띄울 수 없습니다."));
		return;
	}

	UClass* WidgetClass = SettingsWidgetClass.IsValid() ? SettingsWidgetClass.TryLoadClass<UUserWidget>() : nullptr;
	if (WidgetClass == nullptr)
	{
		UE_LOG(LogTRNet, Warning,
			TEXT("음성 설정 패널: WBP 가 지정되지 않았습니다. DefaultGame.ini 의 [/Script/TR.TRVoiceSubsystem] SettingsWidgetClass 를 채우세요. 콘솔 명령은 그대로 씁니다."));
		return;
	}

	SettingsWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (SettingsWidget != nullptr)
	{
		SettingsWidget->AddToViewport(1000);
	}
}

//--- 콘솔 명령 ---

void UTRVoiceSubsystem::RegisterConsoleCommands()
{
	IConsoleManager& Manager = IConsoleManager::Get();

	// PIE 는 인스턴스마다 서브시스템이 생긴다. 명령은 한 벌만 두고, 실행 시점에
	// GWorld 로 대상을 다시 찾는다.
	if (Manager.FindConsoleObject(TEXT("TR.Voice.Status")) != nullptr)
	{
		return;
	}

	auto AddCommand = [this, &Manager](const TCHAR* Name, const TCHAR* Help, TFunction<void(UTRVoiceSubsystem&, const TArray<FString>&)> Body)
	{
		IConsoleObject* Object = Manager.RegisterConsoleCommand(Name, Help,
			FConsoleCommandWithArgsDelegate::CreateLambda([Body](const TArray<FString>& Args)
			{
				if (UTRVoiceSubsystem* Target = UTRVoiceSubsystem::Get(GWorld))
				{
					Body(*Target, Args);
				}
			}),
			ECVF_Default);

		if (Object != nullptr)
		{
			ConsoleCommands.Add(Object);
		}
	};

	AddCommand(TEXT("TR.Voice.Status"), TEXT("음성 상태를 로그로 출력한다."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>&)
		{
			UE_LOG(LogTRNet, Log, TEXT("\n%s"), *Voice.GetStatusText());
		});

	AddCommand(TEXT("TR.Voice.UI"), TEXT("임시 음성 설정 패널을 토글한다."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>&)
		{
			Voice.ToggleSettingsUI();
		});

	AddCommand(TEXT("TR.Voice.PushToTalk"), TEXT("1 = 무전, 0 = 오픈마이크."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>& Args)
		{
			Voice.SetPushToTalk(Args.Num() > 0 ? (FCString::Atoi(*Args[0]) != 0) : true);
		});

	AddCommand(TEXT("TR.Voice.MicGain"), TEXT("마이크 증폭 0~4."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>& Args)
		{
			if (Args.Num() > 0) { Voice.SetMicGain(FCString::Atof(*Args[0])); }
		});

	AddCommand(TEXT("TR.Voice.MicThreshold"), TEXT("무음 판정 문턱 0~1. 소리가 안 나가면 낮춘다."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>& Args)
		{
			if (Args.Num() > 0) { Voice.SetMicThreshold(FCString::Atof(*Args[0])); }
		});

	AddCommand(TEXT("TR.Voice.MicNoiseGate"), TEXT("노이즈 게이트 0~1."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>& Args)
		{
			if (Args.Num() > 0) { Voice.SetMicNoiseGate(FCString::Atof(*Args[0])); }
		});

	AddCommand(TEXT("TR.Voice.OutputMuted"), TEXT("1 = 남의 음성 출력을 끈다."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>& Args)
		{
			Voice.SetOutputMuted(Args.Num() > 0 ? (FCString::Atoi(*Args[0]) != 0) : false);
		});

	AddCommand(TEXT("TR.Voice.JitterDelay"), TEXT("수신 버퍼 지연(초). 끊기면 올린다."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>& Args)
		{
			if (Args.Num() > 0) { Voice.SetJitterDelay(FCString::Atof(*Args[0])); }
		});

	AddCommand(TEXT("TR.Voice.Talk"), TEXT("키 없이 송신을 켜고 끈다. 1 = 송신."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>& Args)
		{
			const bool bTalk = Args.Num() > 0 ? (FCString::Atoi(*Args[0]) != 0) : true;
			if (UTRVoiceComponent* Component = Voice.GetLocalVoiceComponent())
			{
				if (bTalk) { Component->StartPushToTalk(); }
				else { Component->StopPushToTalk(); }
			}
		});

	AddCommand(TEXT("TR.Voice.Save"), TEXT("현재 값을 GameUserSettings.ini 에 저장한다."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>&)
		{
			Voice.SaveVoiceSettings();
		});

	AddCommand(TEXT("TR.Voice.Reset"), TEXT("기본값으로 되돌린다(저장은 따로)."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>&)
		{
			Voice.ResetVoiceSettings();
		});
}

void UTRVoiceSubsystem::UnregisterConsoleCommands()
{
	for (IConsoleObject* Object : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Object);
	}
	ConsoleCommands.Empty();
}

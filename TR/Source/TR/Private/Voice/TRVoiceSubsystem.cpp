// TRVoiceSubsystem.cpp

#include "Voice/TRVoiceSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/VoiceInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Session/TRNetLog.h"
#include "UObject/UObjectGlobals.h"
#include "VoiceInterfaceImpl.h"
#include "Voice/TRVoiceComponent.h"
#include "Voice/TRVoiceTalker.h"

namespace
{
	// FOnlineVoiceImpl 의 protected VoiceEngine 을 파생 타입을 통해 읽는다.
	struct FTRVoiceEngineAccess : public FOnlineVoiceImpl
	{
		static IVoiceEnginePtr Get(IOnlineVoice& Voice)
		{
			return static_cast<FTRVoiceEngineAccess&>(Voice).*(&FTRVoiceEngineAccess::VoiceEngine);
		}
	};

	IOnlineVoicePtr GetVoiceInterface(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		IOnlineSubsystem* OSS = Online::GetSubsystem(World);
		return OSS ? OSS->GetVoiceInterface() : nullptr;
	}
}

namespace TRVoiceCVar
{
	// 음성 조절용 콘솔 변수 이름.
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

	// 패널 WBP 경로는 DefaultGame.ini 에서 읽는다.
	FString WidgetPath;
	if (GConfig->GetString(TEXT("/Script/TR.TRVoiceSubsystem"), TEXT("SettingsWidgetClass"), WidgetPath, GGameIni)
		&& !WidgetPath.IsEmpty())
	{
		SettingsWidgetClass = FSoftClassPath(WidgetPath);
	}

	ApplyToRuntime();

	// 로컬 플레이어가 생길 때 컴포넌트를 붙인다.
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

	// 송신 전에 토커 등록과 델리게이트 구독을 마친다.
	BindVoiceInterfaceDelegates();
	EnsureVoiceComponent(LocalPlayer->PlayerController);
}

uint32 UTRVoiceSubsystem::GetLocalUserNum() const
{
	const ULocalPlayer* LP = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	return LP ? (uint32)LP->GetControllerId() : 0;
}

bool UTRVoiceSubsystem::EnsureLocalTalkerRegistered()
{
	const IOnlineVoicePtr Voice = GetVoiceInterface(this);
	if (!Voice.IsValid())
	{
		return false;
	}

	return Voice->RegisterLocalTalker(GetLocalUserNum());
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

	// 로컬 토커 등록. 발화 알림·송신·마이크 진폭에 필요하다.
	EnsureLocalTalkerRegistered();
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
		Voice->UnregisterLocalTalker(GetLocalUserNum());
	}
	TalkingStateHandle.Reset();
}

void UTRVoiceSubsystem::HandleTalkingStateChanged(FUniqueNetIdRef PlayerId, bool bIsTalking)
{
	// 발화 시작/종료 로그. 수신 판정용.
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

	// 키 바인딩을 다시 시도한다.
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

void UTRVoiceSubsystem::SetOutputVolume(float Value)
{
	OutputVolume = FMath::Clamp(Value, 0.f, 2.f);
	ApplyAllPlayerVolumes();
	OnVoiceSettingsChanged.Broadcast();
}

//--- 개인별 음량 ---

float UTRVoiceSubsystem::GetPlayerVolume(const FString& NetId) const
{
	const float* Found = PlayerVolumes.Find(NetId);
	return Found ? *Found : 1.f;
}

void UTRVoiceSubsystem::SetPlayerVolume(const FString& NetId, float Value)
{
	if (NetId.IsEmpty())
	{
		return;
	}

	PlayerVolumes.Add(NetId, FMath::Clamp(Value, 0.f, 2.f));
	ApplyAllPlayerVolumes();
	OnVoiceSettingsChanged.Broadcast();
}

float UTRVoiceSubsystem::GetEffectiveVolume(const FString& NetId) const
{
	return OutputVolume * GetPlayerVolume(NetId);
}

UTRVoiceTalker* UTRVoiceSubsystem::EnsureTalker(APlayerState* PlayerState)
{
	if (PlayerState == nullptr || !PlayerState->GetUniqueId().IsValid())
	{
		return nullptr;
	}

	UTRVoiceTalker* Talker = PlayerState->FindComponentByClass<UTRVoiceTalker>();
	if (Talker == nullptr)
	{
		Talker = NewObject<UTRVoiceTalker>(PlayerState, UTRVoiceTalker::StaticClass(), TEXT("TRVoiceTalker"));
		Talker->RegisterComponent();
		Talker->RegisterWithPlayerState(PlayerState);
		Talker->ApplyVolume(GetEffectiveVolume((*PlayerState->GetUniqueId()).ToString()));
	}
	return Talker;
}

void UTRVoiceSubsystem::ApplyAllPlayerVolumes()
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState == nullptr)
	{
		return;
	}

	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (UTRVoiceTalker* Talker = EnsureTalker(PS))
		{
			Talker->ApplyVolume(GetEffectiveVolume((*PS->GetUniqueId()).ToString()));
		}
	}
}

TArray<FTRVoicePlayerInfo> UTRVoiceSubsystem::GetRemotePlayers()
{
	TArray<FTRVoicePlayerInfo> Result;

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState == nullptr)
	{
		return Result;
	}

	const APlayerController* LocalPC = GetLocalController();
	const APlayerState* LocalPS = LocalPC ? LocalPC->PlayerState : nullptr;
	const FUniqueNetIdRepl LocalId = LocalPS ? LocalPS->GetUniqueId() : FUniqueNetIdRepl();
	const IOnlineVoicePtr Voice = GetVoiceInterface(this);

	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (PS == nullptr || PS->IsInactive() || !PS->GetUniqueId().IsValid())
		{
			continue;
		}

		// 나 자신은 뺀다. 같은 객체, 로컬 컨트롤러 소유, 같은 UniqueId 중 하나면 나다.
		const APlayerController* OwnerPC = PS->GetPlayerController();
		const bool bIsLocal = (PS == LocalPS)
			|| (OwnerPC != nullptr && OwnerPC->IsLocalController())
			|| (LocalId.IsValid() && *LocalId == *PS->GetUniqueId());
		if (bIsLocal)
		{
			continue;
		}

		EnsureTalker(PS);

		const FUniqueNetIdRepl& Id = PS->GetUniqueId();
		FTRVoicePlayerInfo& Info = Result.AddDefaulted_GetRef();
		Info.NetId = (*Id).ToString();
		Info.DisplayName = PS->GetPlayerName();
		Info.Volume = GetPlayerVolume(Info.NetId);
		if (Voice.IsValid())
		{
			Info.bIsTalking = Voice->IsRemotePlayerTalking(*Id);
			Info.Level = FMath::Max(0.f, Voice->GetAmplitudeOfRemoteTalker(*Id));
		}
	}

	return Result;
}

float UTRVoiceSubsystem::GetLocalMicLevel() const
{
	const IOnlineVoicePtr Voice = GetVoiceInterface(this);
	if (!Voice.IsValid())
	{
		return 0.f;
	}

	const IVoiceEnginePtr Engine = FTRVoiceEngineAccess::Get(*Voice);
	if (!Engine.IsValid())
	{
		return 0.f;
	}

	// 게이지는 증폭이 적용된 크기를 보여 준다. 지원 안 되면 -1 이 온다.
	const float Raw = Engine->GetMicrophoneAmplitude((int32)GetLocalUserNum());
	return FMath::Clamp(Raw * MicGain, 0.f, 1.f);
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
	OutputVolume = Defaults->OutputVolume;
	PlayerVolumes.Empty();
	ApplyAllPlayerVolumes();

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

FString UTRVoiceSubsystem::GetStatusText(bool bIncludeEngineDump) const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	const IOnlineVoicePtr Voice = OSS ? OSS->GetVoiceInterface() : nullptr;
	const UTRVoiceComponent* Component = GetLocalVoiceComponent();

	FString Text = FString::Printf(
		TEXT("OSS=%s 인터페이스=%s 무전키=%s 바인딩=%s\n")
		TEXT("송신모드=%s 송신중=%s 증폭=%.2f 문턱=%.3f 노이즈게이트=%.3f\n")
		TEXT("출력음소거=%s 수신음량=%.2f 지터=%.2fs"),
		OSS ? *OSS->GetSubsystemName().ToString() : TEXT("없음"),
		Voice.IsValid() ? TEXT("있음") : TEXT("없음"),
		Component ? *Component->GetPushToTalkKey().ToString() : TEXT("컴포넌트없음"),
		(Component && Component->IsBound()) ? TEXT("완료") : TEXT("미완료"),
		bPushToTalk ? TEXT("무전") : TEXT("오픈마이크"),
		IsLocalTalking() ? TEXT("예") : TEXT("아니오"),
		MicGain, MicThreshold, MicNoiseGate,
		bOutputMuted ? TEXT("켜짐") : TEXT("꺼짐"),
		OutputVolume, JitterDelay);

	if (bIncludeEngineDump && Voice.IsValid())
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

			// 열기 전에 커서가 숨겨져 있었으면 다시 숨긴다.
			APlayerController* PC = GetLocalController();
			if (PC != nullptr && !bCursorWasVisible)
			{
				FInputModeGameOnly Mode;
				Mode.SetConsumeCaptureMouseDown(true);
				PC->SetInputMode(Mode);
				PC->SetShowMouseCursor(false);
			}
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
	if (SettingsWidget == nullptr)
	{
		return;
	}

	SettingsWidget->AddToViewport(1000);

	// 패널이 열린 동안 커서와 UI 입력을 켠다.
	bCursorWasVisible = PC->bShowMouseCursor;
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetWidgetToFocus(SettingsWidget->TakeWidget());
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
}

//--- 콘솔 명령 ---

void UTRVoiceSubsystem::RegisterConsoleCommands()
{
	IConsoleManager& Manager = IConsoleManager::Get();

	// 명령은 한 벌만 등록하고 실행 시점의 GWorld 로 대상을 찾는다.
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

	AddCommand(TEXT("TR.Voice.OutputVolume"), TEXT("수신 음성 전체 음량 0~2."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>& Args)
		{
			if (Args.Num() > 0) { Voice.SetOutputVolume(FCString::Atof(*Args[0])); }
		});

	AddCommand(TEXT("TR.Voice.PlayerVolume"), TEXT("개인 음량. <NetId> <0~2>. NetId 는 TR.Voice.Players 로."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>& Args)
		{
			if (Args.Num() > 1) { Voice.SetPlayerVolume(Args[0], FCString::Atof(*Args[1])); }
		});

	AddCommand(TEXT("TR.Voice.Players"), TEXT("접속자와 개인 음량·발화 상태를 로그로."),
		[](UTRVoiceSubsystem& Voice, const TArray<FString>&)
		{
			for (const FTRVoicePlayerInfo& P : Voice.GetRemotePlayers())
			{
				UE_LOG(LogTRNet, Log, TEXT("접속자 %s (%s) 음량=%.2f 발화=%s 레벨=%.2f"),
					*P.DisplayName, *P.NetId, P.Volume, P.bIsTalking ? TEXT("예") : TEXT("아니오"), P.Level);
			}
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

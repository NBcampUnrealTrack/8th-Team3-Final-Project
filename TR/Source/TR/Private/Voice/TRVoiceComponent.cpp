// TRVoiceComponent.cpp

#include "Voice/TRVoiceComponent.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Session/TRNetLog.h"
#include "TimerManager.h"
#include "Voice/TRVoiceSubsystem.h"

namespace TRVoiceComponentConfig
{
	// 핸드셰이크 RPC 바로 뒤에 오는 송신 끄기 RPC 가 처리될 시간을 준 뒤 모드를 되돌린다.
	static constexpr float ReapplyDelay = 0.3f;
	static constexpr float HandshakePollInterval = 0.1f;
}

UTRVoiceComponent::UTRVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = TRVoiceComponentConfig::HandshakePollInterval;
}

void UTRVoiceComponent::BeginPlay()
{
	Super::BeginPlay();

	BindKeys();

	if (const UTRVoiceSubsystem* Voice = UTRVoiceSubsystem::Get(this))
	{
		ApplyTransmitMode(Voice->IsPushToTalk());
	}
}

void UTRVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APlayerController* PC = GetOwningController();
	if (PC == nullptr || bHandshakeSeen || !PC->MuteList.bHasVoiceHandshakeCompleted)
	{
		return;
	}

	bHandshakeSeen = true;
	SetComponentTickEnabled(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReapplyTimer, this, &UTRVoiceComponent::ReapplyTransmitMode,
			TRVoiceComponentConfig::ReapplyDelay, false);
	}
}

void UTRVoiceComponent::ReapplyTransmitMode()
{
	if (const UTRVoiceSubsystem* Voice = UTRVoiceSubsystem::Get(this))
	{
		ApplyTransmitMode(Voice->IsPushToTalk());
	}
}

APlayerController* UTRVoiceComponent::GetOwningController() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UTRVoiceComponent::BindKeys()
{
	if (bBound)
	{
		return true;
	}

	APlayerController* PC = GetOwningController();
	if (PC == nullptr || PC->InputComponent == nullptr)
	{
		return false;
	}

	// 컨트롤러 입력 컴포넌트에 직접 건다. 폰이 없어도 동작한다.
	if (PushToTalkKey.IsValid())
	{
		PC->InputComponent->BindKey(PushToTalkKey, IE_Pressed, this, &UTRVoiceComponent::StartPushToTalk);
		PC->InputComponent->BindKey(PushToTalkKey, IE_Released, this, &UTRVoiceComponent::StopPushToTalk);
	}

	if (SettingsUIKey.IsValid())
	{
		PC->InputComponent->BindKey(SettingsUIKey, IE_Pressed, this, &UTRVoiceComponent::ToggleSettingsUI);
	}

	bBound = true;

	UE_LOG(LogTRNet, Log, TEXT("무전 키 연결 — 송신=%s 설정패널=%s"),
		*PushToTalkKey.ToString(), *SettingsUIKey.ToString());

	return true;
}

void UTRVoiceComponent::StartPushToTalk()
{
	bPushToTalkHeld = true;

	// 송신 전에 로컬 토커를 등록한다.
	if (UTRVoiceSubsystem* Voice = UTRVoiceSubsystem::Get(this))
	{
		Voice->EnsureLocalTalkerRegistered();
	}

	if (APlayerController* PC = GetOwningController())
	{
		PC->StartTalking();
	}
}

void UTRVoiceComponent::StopPushToTalk()
{
	bPushToTalkHeld = false;

	// 오픈마이크에서는 키를 떼도 송신을 닫지 않는다.
	const UTRVoiceSubsystem* Voice = UTRVoiceSubsystem::Get(this);
	if (Voice != nullptr && !Voice->IsPushToTalk())
	{
		return;
	}

	if (APlayerController* PC = GetOwningController())
	{
		PC->StopTalking();
	}
}

void UTRVoiceComponent::ApplyTransmitMode(bool bInPushToTalk)
{
	APlayerController* PC = GetOwningController();
	if (PC == nullptr)
	{
		return;
	}

	if (bInPushToTalk)
	{
		if (!bPushToTalkHeld)
		{
			PC->StopTalking();
		}
		return;
	}

	if (UTRVoiceSubsystem* Voice = UTRVoiceSubsystem::Get(this))
	{
		Voice->EnsureLocalTalkerRegistered();
	}
	PC->StartTalking();
}

void UTRVoiceComponent::ToggleSettingsUI()
{
	if (UTRVoiceSubsystem* Voice = UTRVoiceSubsystem::Get(this))
	{
		Voice->ToggleSettingsUI();
	}
}

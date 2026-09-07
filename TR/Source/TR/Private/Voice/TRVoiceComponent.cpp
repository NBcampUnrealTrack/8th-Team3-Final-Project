// TRVoiceComponent.cpp

#include "Voice/TRVoiceComponent.h"

#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Session/TRNetLog.h"
#include "Voice/TRVoiceSubsystem.h"

UTRVoiceComponent::UTRVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

	// Enhanced Input 이 아니라 키 바인딩을 쓴다. IMC 는 캐릭터가 들고 있어 폰이 있어야
	// 살아나지만, 무전은 로비·사망·관전 중에도 눌려야 한다.
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

	PC->StartTalking();
}

void UTRVoiceComponent::ToggleSettingsUI()
{
	if (UTRVoiceSubsystem* Voice = UTRVoiceSubsystem::Get(this))
	{
		Voice->ToggleSettingsUI();
	}
}

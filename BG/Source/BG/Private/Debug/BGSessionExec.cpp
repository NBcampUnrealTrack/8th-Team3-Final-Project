#include "Core/BGGameInstance.h"
#include "Session/BGSessionFlow.h"
#include "Debug/BGNetLog.h"
#include "UI/Session/BGSessionMenuController.h"
#include "GameFramework/PlayerController.h"

#if !UE_BUILD_SHIPPING
namespace
{
	IBGSessionInterface* GetSession(UGameInstance* GI)
	{
		UBGSessionFlow* Flow = GI ? GI->GetSubsystem<UBGSessionFlow>() : nullptr;
		if (!Flow)
		{
			UE_LOG(LogBGNet, Warning, TEXT("[Exec] BGSessionFlow 서브시스템 없음"));
		}
		return Flow;
	}
}
#endif

void UBGGameInstance::BG_Host()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBGNet, Log, TEXT("[Exec] BG_Host"));
	if (IBGSessionInterface* Session = GetSession(this))
	{
		Session->HostCreateRoom(FString());
	}
#endif
}

void UBGGameInstance::BG_Find()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBGNet, Log, TEXT("[Exec] BG_Find"));
	if (IBGSessionInterface* Session = GetSession(this))
	{
		Session->RefreshSessionList(FBGSessionSearchFilter());
	}
#endif
}

void UBGGameInstance::BG_Join()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBGNet, Log, TEXT("[Exec] BG_Join (index 0)"));
	if (IBGSessionInterface* Session = GetSession(this))
	{
		Session->JoinSessionByIndex(0);
	}
#endif
}

void UBGGameInstance::BG_JoinCode(const FString& Code)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBGNet, Log, TEXT("[Exec] BG_JoinCode '%s'"), *Code);
	if (IBGSessionInterface* Session = GetSession(this))
	{
		Session->JoinRoomByCode(Code);
	}
#endif
}

void UBGGameInstance::BG_Leave()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBGNet, Log, TEXT("[Exec] BG_Leave"));
	if (IBGSessionInterface* Session = GetSession(this))
	{
		Session->LeaveSession();
	}
#endif
}

void UBGGameInstance::BG_Ready(int32 bReady)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBGNet, Log, TEXT("[Exec] BG_Ready %d"), bReady);
	if (IBGSessionInterface* Session = GetSession(this))
	{
		Session->SetLocalReady(bReady != 0);
	}
#endif
}

void UBGGameInstance::BG_Start()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBGNet, Log, TEXT("[Exec] BG_Start"));
	if (IBGSessionInterface* Session = GetSession(this))
	{
		Session->StartGame();
	}
#endif
}

void UBGGameInstance::BG_RoomCode()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBGNet, Log, TEXT("[Exec] BG_RoomCode = '%s'"), *GetHostRoomCode());
#endif
}

void UBGGameInstance::BG_UI()
{
#if !UE_BUILD_SHIPPING
	if (UBGSessionMenuController* Menu = GetSubsystem<UBGSessionMenuController>())
	{
		Menu->Toggle();
	}
#endif
}

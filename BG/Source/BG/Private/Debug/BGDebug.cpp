#include "Debug/BGDebug.h"

#include "Debug/BGNetLog.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Lobby/BGMatchStartComponent.h"
#include "Lobby/BGReadyComponent.h"

static TAutoConsoleVariable<int32> CVarBGDebug(
	TEXT("BG.Debug"),
	1,
	TEXT("BANG! 화면 디버그 메시지 표시 (0=끄기, 1=켜기)."),
	ECVF_Default);

bool BGDebug::IsEnabled()
{
	return CVarBGDebug.GetValueOnAnyThread() != 0;
}

void BGDebug::Screen(const FString& Msg, const FColor& Color, float Time, int32 Key)
{
	// 화면 메시지와 별개로 로그에도 항상 남긴다(로그만 보고 싶을 때/서버 없는 환경 대비).
	UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);

	if (IsEnabled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(Key, Time, Color, Msg);
	}
}

void BGDebug::TryAutoStartMatch(UBGMatchStartComponent* MatchStart, int32 Total, int32 Required)
{
#if !UE_BUILD_SHIPPING
	if (!MatchStart || Total < Required)
	{
		return;
	}
	if (!FParse::Param(FCommandLine::Get(), TEXT("BGAutoStart")))
	{
		return;
	}

	const UWorld* World = MatchStart->GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	for (APlayerState* PS : (GS ? GS->PlayerArray : TArray<APlayerState*>()))
	{
		if (UBGReadyComponent* Ready = UBGReadyComponent::Find(PS))
		{
			Ready->SetReadyAuthoritative(true);
		}
	}

	UE_LOG(LogBGNet, Warning, TEXT("[MatchStart] -BGAutoStart — 자동 시작"));
	MatchStart->StartMatch();
#endif
}

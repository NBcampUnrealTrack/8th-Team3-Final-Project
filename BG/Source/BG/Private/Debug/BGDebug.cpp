#include "Debug/BGDebug.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

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

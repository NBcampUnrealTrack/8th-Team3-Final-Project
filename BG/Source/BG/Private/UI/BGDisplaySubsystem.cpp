#include "UI/BGDisplaySubsystem.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "UI/Widget/BGSystemMenuWidget.h"

void UBGDisplaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// PIE 는 에디터 뷰포트 안에서 돈다 — 화면 모드를 건드리면 에디터 창이 바뀐다.
	if (GIsEditor || IsRunningDedicatedServer())
	{
		return;
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UBGDisplaySubsystem::HandleFirstTick));
}

void UBGDisplaySubsystem::Deinitialize()
{
	FTSTicker::RemoveTicker(TickHandle);
	Super::Deinitialize();
}

bool UBGDisplaySubsystem::HandleFirstTick(float DeltaTime)
{
	TickHandle.Reset();

	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return false;
	}

	Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);

	// 전체 창 모드는 엔진이 데스크톱 해상도로 그려 ResolutionSizeX/Y 를 보지 않는다.
	// 저장값도 모니터 해상도로 맞춰 둬야 나중에 전체 화면으로 바꿀 때 옛 값이 되살아나지 않는다.
	const FIntPoint Native = UBGSystemMenuWidget::GetCurrentMonitorResolution();
	if (Native.X > 0 && Native.Y > 0)
	{
		Settings->SetScreenResolution(Native);
	}

	// 인자 false — 실행 인자(-windowed, -ResX)를 다시 끼워 넣지 않는다.
	Settings->ApplySettings(false);
	return false;
}

#include "UI/Session/TRSessionMenuController.h"

#include "Session/TRNetLog.h"
#include "UI/Session/TRSessionMenuWidget.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UTRSessionMenuController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadConfig();

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UTRSessionMenuController::HandlePostLoadMap);
}

void UTRSessionMenuController::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

	Super::Deinitialize();
}

void UTRSessionMenuController::Toggle()
{
	if (Menu)
	{
		Close();
	}
	else
	{
		Open();
	}
}

void UTRSessionMenuController::Open()
{
	if (Menu)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!ensureMsgf(PC, TEXT("로컬 PlayerController 없이 세션 메뉴를 열려고 했다")))
	{
		return;
	}

	// WBP 를 못 찾으면 열지 않는다. 조용히 넘어가면 F9 가 먹통인 이유를 알 수 없다.
	TSubclassOf<UTRSessionMenuWidget> Class = MenuWidgetClass.LoadSynchronous();
	if (!ensureMsgf(Class, TEXT("세션 메뉴 WBP 로드 실패: %s"), *MenuWidgetClass.ToString()))
	{
		return;
	}

	Menu = CreateWidget<UTRSessionMenuWidget>(PC, Class);
	if (!ensure(Menu))
	{
		return;
	}

	// 입력 모드와 커서는 위젯의 NativeConstruct 가 잡는다.
	Menu->AddToViewport(100);
	bWasOpen = true;

	UE_LOG(LogTRNet, Log, TEXT("[SessionMenu] 열림"));
}

void UTRSessionMenuController::Close()
{
	// 입력 모드 복원은 위젯의 NativeDestruct 가 한다.
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}

	bWasOpen = false;
	UE_LOG(LogTRNet, Log, TEXT("[SessionMenu] 닫음"));
}

void UTRSessionMenuController::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// 트래블이 위젯을 파괴했다. 참조만 비우고 열려 있었다는 사실은 남긴다.
	Menu = nullptr;

	if (!LoadedWorld)
	{
		return;
	}

	//	세션에 들어왔으면 메뉴는 할 일을 마쳤다. 게임 화면 위에 다시 띄우지 않는다.
	const ENetMode NetMode = LoadedWorld->GetNetMode();
	if (NetMode == NM_Client || NetMode == NM_ListenServer)
	{
		bWasOpen = false;
		return;
	}

	//	여는 키는 DebugExecBindings 가 맡으므로 여기서 다시 걸 것이 없다.
	//	PlayerController 는 맵 로드 직후에는 아직 없어 위젯 복구만 한 틱 미룬다.
	if (!bWasOpen)
	{
		return;
	}

	LoadedWorld->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (bWasOpen && !Menu)
			{
				Open();
			}
		}));
}

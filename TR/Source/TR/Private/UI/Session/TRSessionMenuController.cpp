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

	// 트래블 전에 닫아야 위젯의 NativeDestruct 가 입력 모드와 커서를 되돌린다.
	// PostLoadMap 에서 참조만 버리면 복원이 통째로 건너뛰어져 시점이 잠긴 채 남는다.
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(
		this, &UTRSessionMenuController::HandlePreLoadMap);
}

void UTRSessionMenuController::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);

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
	bWasOpen = false;

	if (!Menu)
	{
		return;
	}

	// 입력 모드 복원은 위젯의 NativeDestruct 가 한다.
	Menu->RemoveFromParent();
	Menu = nullptr;

	UE_LOG(LogTRNet, Log, TEXT("[SessionMenu] 닫음"));
}

void UTRSessionMenuController::HandlePreLoadMap(const FString& MapName)
{
	// 열려 있던 상태는 남긴다. 트래블 뒤 복구 판단은 HandlePostLoadMap 이 한다.
	const bool bOpen = (Menu != nullptr);

	Close();
	bWasOpen = bOpen;
}

void UTRSessionMenuController::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// PreLoadMap 이 이미 닫았지만, 트래블 없이 맵이 바뀌는 경로도 있어 한 번 더 정리한다.
	if (Menu)
	{
		Menu->RemoveFromParent();
	}

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

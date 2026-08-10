#include "UI/Session/TRSessionMenuController.h"

#include "Session/TRNetLog.h"
#include "UI/Session/TRSessionMenuWidget.h"

#include "Components/InputComponent.h"
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

	// PlayerController 는 맵 로드 직후에는 아직 없다. 한 틱 뒤에 처리한다.
	LoadedWorld->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			BindDebugKey();

			if (bWasOpen && !Menu)
			{
				Open();
			}
		}));
}

void UTRSessionMenuController::BindDebugKey()
{
	if (!DebugToggleKey.IsValid())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC || !PC->InputComponent)
	{
		return;   // 아직 없다. 다음 맵 로드에서 다시 시도한다.
	}

	// 컨트롤러는 맵마다 새로 만들어지므로 중복 바인딩 걱정은 없다.
	PC->InputComponent->BindKey(DebugToggleKey, IE_Pressed, this, &UTRSessionMenuController::Toggle);

	UE_LOG(LogTRNet, Log, TEXT("[SessionMenu] 디버그 키 %s 연결"), *DebugToggleKey.ToString());
}

#include "UI/Session/BGSessionMenuController.h"

#include "UI/Session/BGSessionMenuWidget.h"
#include "Debug/BGNetLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UBGSessionMenuController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadConfig();

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UBGSessionMenuController::HandlePostLoadMap);
}

void UBGSessionMenuController::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Deinitialize();
}

void UBGSessionMenuController::Toggle()
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

void UBGSessionMenuController::Open()
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		UE_LOG(LogBGNet, Warning, TEXT("[SessionMenu] 로컬 PlayerController 없음"));
		return;
	}

	TSubclassOf<UBGSessionMenuWidget> Class = MenuWidgetClass.LoadSynchronous();
	if (!Class)
	{
		Class = UBGSessionMenuWidget::StaticClass();
		UE_LOG(LogBGNet, Warning, TEXT("[SessionMenu] '%s' 로드 실패 — C++ 기본 트리 사용"),
			*MenuWidgetClass.ToString());
	}

	Menu = CreateWidget<UBGSessionMenuWidget>(PC, Class);
	if (!Menu)
	{
		UE_LOG(LogBGNet, Warning, TEXT("[SessionMenu] 위젯 생성 실패"));
		return;
	}

	// 입력 모드/커서는 위젯의 NativeConstruct 가 잡는다.
	Menu->AddToViewport(100);
	bOpen = true;
	UE_LOG(LogBGNet, Log, TEXT("[SessionMenu] 열림"));
}

void UBGSessionMenuController::Close()
{
	// 입력 모드 복원은 위젯의 NativeDestruct 가 한다.
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}
	bOpen = false;
	UE_LOG(LogBGNet, Log, TEXT("[SessionMenu] 닫음"));
}

void UBGSessionMenuController::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// 조인은 ClientTravel 이라 월드가 갈리고 위젯도 함께 파괴된다.
	Menu = nullptr;

	if (!bOpen || !LoadedWorld)
	{
		return;
	}

	// 이 시점엔 로컬 PlayerController 가 아직 없을 수 있다.
	LoadedWorld->GetTimerManager().SetTimer(RestoreTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (bOpen && !Menu)
			{
				Open();
			}
		}), 0.2f, /*bLoop=*/false);
}

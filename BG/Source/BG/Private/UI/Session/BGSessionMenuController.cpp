#include "UI/Session/BGSessionMenuController.h"

#include "UI/Session/BGSessionMenuWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "BGGameplayTags.h"
#include "Core/BGGameMode.h"     // EBGPlayMode - PvE 에서는 자동 오픈을 건너뛴다
#include "Core/BGGameState.h"
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
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(RestoreTimer);
	}
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
	//	이미 떠 있으면 만들지 않는다. 두 번 만들면 첫 위젯이 Close() 로 못 지우는 고아가 된다.
	if (Menu)
	{
		return;
	}

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

	//	Menu 가 놓친 고아까지 걷어낸다.
	RemoveStrayMenus();

	bOpen = false;
	UE_LOG(LogBGNet, Log, TEXT("[SessionMenu] 닫음"));
}

void UBGSessionMenuController::RemoveStrayMenus()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Found, UBGSessionMenuWidget::StaticClass(), false);

	int32 Removed = 0;
	for (UUserWidget* Widget : Found)
	{
		if (Widget && Widget->IsInViewport())
		{
			Widget->RemoveFromParent();
			++Removed;
		}
	}

	if (Removed > 0)
	{
		UE_LOG(LogBGNet, Warning, TEXT("[SessionMenu] 남아 있던 위젯 %d개 제거"), Removed);
	}
}

void UBGSessionMenuController::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// 조인은 ClientTravel 이라 월드가 갈리고 위젯도 함께 파괴된다.
	Menu = nullptr;

	if (!LoadedWorld)
	{
		return;
	}

	//	PostLoadMap 은 전역 통지라 PIE 에선 다른 인스턴스의 월드도 온다 — 내 GameInstance 월드만 받는다.
	UGameInstance* GI = GetGameInstance();
	if (!GI || LoadedWorld->GetGameInstance() != GI)
	{
		return;
	}

	//	PlayerController 와 GameState 가 올 때까지 짧게 재시도한다.
	//	타이머 매니저는 GameInstance 소유라 트래블 뒤에도 돈다 — 이전 요청은 먼저 끊고, 월드는 약참조로 잡는다.
	GI->GetTimerManager().ClearTimer(RestoreTimer);
	RestoreAttempts = 0;
	TWeakObjectPtr<UWorld> WeakWorld(LoadedWorld);
	GI->GetTimerManager().SetTimer(RestoreTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, WeakWorld]()
		{
			UGameInstance* OwnerGI = GetGameInstance();
			if (!OwnerGI)
			{
				return;
			}
			if (!WeakWorld.IsValid() || OwnerGI->GetWorld() != WeakWorld.Get())
			{
				OwnerGI->GetTimerManager().ClearTimer(RestoreTimer);
				return;
			}
			TryRestoreAfterTravel(WeakWorld.Get());
		}),
		0.2f, /*bLoop=*/true);
}

void UBGSessionMenuController::TryRestoreAfterTravel(UWorld* World)
{
	if (!IsValid(World))
	{
		return;
	}

	const auto Stop = [this, World]()
	{
		World->GetTimerManager().ClearTimer(RestoreTimer);
	};

	if (Menu)
	{
		Stop();
		return;
	}

	//	열려 있던 채로 트래블했으면 그대로 되살린다.
	if (bOpen)
	{
		Open();
		Stop();
		return;
	}

	//	로비로 돌아왔으면 준비를 다시 누를 수 있게 띄운다.
	//	PvE 는 제외 - 방 만들기를 누르면 ?listen 트래블에 PlayMode 가 빠져 PvP 로 되돌아간다.
	const ABGGameState* GS = World->GetGameState<ABGGameState>();
	if (GS && GS->GetPhaseTag().IsValid())
	{
		const bool bPvE = ABGGameMode::GetPlayMode(World) == EBGPlayMode::PvE;

		if (!bPvE && GS->GetPhaseTag() == BG::Phase::Lobby.GetTag())
		{
			Open();
		}
		Stop();
		return;
	}

	//	GameState 가 아직 없다. 정해진 횟수까지만 재시도한다.
	if (++RestoreAttempts >= MaxRestoreAttempts)
	{
		Stop();
	}
}

// TRPlayerHUD.cpp

#include "UI/TRPlayerHUD.h"
#include "Blueprint/UserWidget.h"
#include "Debug/TRGameplayLog.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Widgets/SViewport.h"
#include "UI/TRPlayerHUDWidget.h"
#include "UI/TRPopupStack.h"
#include "UI/TRControlConsoleWidget.h"
#include "Core/TRGameState.h"

static UGameViewportClient* GetViewportClient(const APlayerController* PC)
{
	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
}

void ATRPlayerHUD::ApplyGameInputMode(APlayerController* PC)
{
	if (!IsValid(PC))
	{
		return;
	}

	FInputModeGameOnly Mode;
	Mode.SetConsumeCaptureMouseDown(true);

	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(false);

	const UGameViewportClient* ViewportClient = GetViewportClient(PC);
	const TSharedPtr<SViewport> ViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr;
	if (ViewportWidget.IsValid())
	{
		PC->GetLocalPlayer()->GetSlateOperations().CaptureMouse(ViewportWidget.ToSharedRef());
	}

	if (ATRPlayerHUD* HUD = PC->GetHUD<ATRPlayerHUD>())
	{
		HUD->bCenterMouse = true;
	}
}

UTRPlayerHUDWidget* ATRPlayerHUD::FindPlayerHUDWidget(const APawn* Pawn)
{
	const APlayerController* PC = IsValid(Pawn) ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const ATRPlayerHUD* PlayerHUD = IsValid(PC) ? Cast<ATRPlayerHUD>(PC->GetHUD()) : nullptr;

	return IsValid(PlayerHUD) ? PlayerHUD->GetPlayerHUDWidget() : nullptr;
}

void ATRPlayerHUD::PushPopupInputMode(APlayerController* PC, UUserWidget* FocusWidget)
{
	if (UTRPopupStack* PopupStack = UTRPopupStack::Get(PC))
	{
		PopupStack->Push();
	}

	ApplyPopupInputMode(PC, FocusWidget);
}

void ATRPlayerHUD::PopPopupInputMode(APlayerController* PC)
{
	UTRPopupStack* PopupStack = UTRPopupStack::Get(PC);

	// 아직 다른 팝업이 남아 있으면 커서를 그대로 둔다.
	if (PopupStack != nullptr && !PopupStack->Pop())
	{
		ApplyPopupInputMode(PC, nullptr);
		return;
	}

	ApplyGameInputMode(PC);
}

void ATRPlayerHUD::ApplyPopupInputMode(APlayerController* PC, UUserWidget* FocusWidget)
{
	if (!IsValid(PC))
	{
		return;
	}

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (IsValid(FocusWidget))
	{
		Mode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}

	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);

	if (ATRPlayerHUD* HUD = PC->GetHUD<ATRPlayerHUD>())
	{
		HUD->bCenterMouse = false;
	}
}

void ATRPlayerHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = GetOwningPlayerController();
	if (!bCenterMouse || !IsValid(PC))
	{
		return;
	}

	// 뷰포트를 다시 클릭하면 캡처가 돌아오고 중앙 고정
	const UGameViewportClient* ViewportClient = GetViewportClient(PC);
	const FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (Viewport == nullptr || !Viewport->HasMouseCapture())
	{
		return;
	}

	// F8(Eject) 로 빠지면 게임은 계속 돌지만 시점은 에디터 카메라가 잡는다.
	// 그 상태에서 커서를 중앙으로 되돌리면 에디터 카메라를 아예 돌릴 수 없다.
	if (ViewportClient->IsSimulateInEditorViewport())
	{
		return;
	}

	int32 SizeX = 0;
	int32 SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	if (SizeX <= 0 || SizeY <= 0)
	{
		return;
	}

	PC->SetMouseLocation(SizeX / 2, SizeY / 2);
}

static constexpr int32 PreMatchZOrder = 50;

// 설정된 HUD 위젯 클래스를 생성해 플레이어 화면에 표시
void ATRPlayerHUD::BeginPlay()
{
	Super::BeginPlay();

	if (const APlayerController* PC = GetOwningPlayerController())
	{
		if (UTRPopupStack* PopupStack = UTRPopupStack::Get(PC))
		{
			PopupStack->Reset();
		}
	}

	BindMatchPhase();
}

void ATRPlayerHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}

	if (ATRGameState* GameState = BoundGameState.Get())
	{
		GameState->OnMatchStartedChanged.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ATRPlayerHUD::BindMatchPhase()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (AGameStateBase* GameState = World->GetGameState())
	{
		HandleGameStateSet(GameState);
		return;
	}

	// 클라이언트에서는 GameState 가 HUD 보다 늦게 도착한다.
	GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &ATRPlayerHUD::HandleGameStateSet);
}

void ATRPlayerHUD::HandleGameStateSet(AGameStateBase* InGameState)
{
	// GameState 가 교체되면 여기가 다시 불린다. 핸들과 이전 구독을 먼저 떼야 통지가 겹치지 않는다.
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
		GameStateSetHandle.Reset();
	}

	ATRGameState* GameState = Cast<ATRGameState>(InGameState);
	if (!IsValid(GameState))
	{
		ApplyMatchPhase(true);
		return;
	}

	if (ATRGameState* Previous = BoundGameState.Get())
	{
		Previous->OnMatchStartedChanged.RemoveAll(this);
	}

	BoundGameState = GameState;
	GameState->OnMatchStartedChanged.AddUObject(this, &ATRPlayerHUD::ApplyMatchPhase);

	ApplyMatchPhase(GameState->IsMatchStarted());
}

void ATRPlayerHUD::ApplyMatchPhase(bool bStarted)
{
	EScreenPhase Wanted = bStarted ? EScreenPhase::Gameplay : EScreenPhase::PreMatch;

	if (Wanted == EScreenPhase::PreMatch && !IsValid(PreMatchWidgetClass))
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("PreMatchWidgetClass is not set"));
		Wanted = EScreenPhase::Gameplay;
	}

	if (AppliedPhase == Wanted)
	{
		return;
	}

	AppliedPhase = Wanted;

	if (Wanted == EScreenPhase::PreMatch)
	{
		ShowPreMatchScreen();
		return;
	}

	ShowGameplayHUD();
}

void ATRPlayerHUD::ShowPreMatchScreen()
{
	APlayerController* PC = GetOwningPlayerController();

	PreMatchWidget = CreateWidget<UUserWidget>(PC, PreMatchWidgetClass);
	if (!IsValid(PreMatchWidget))
	{
		return;
	}

	if (IsValid(PlayerHUDWidget))
	{
		PlayerHUDWidget->RemoveFromParent();
		PlayerHUDWidget = nullptr;
	}

	PreMatchWidget->AddToViewport(PreMatchZOrder);

	PushPopupInputMode(PC, PreMatchWidget);
	bPreMatchInputSuppressed = true;

	if (IsValid(PC))
	{
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
	}
}

void ATRPlayerHUD::ShowGameplayHUD()
{
	APlayerController* PC = GetOwningPlayerController();

	if (IsValid(PreMatchWidget))
	{
		PreMatchWidget->RemoveFromParent();
		PreMatchWidget = nullptr;
	}

	if (bPreMatchInputSuppressed)
	{
		bPreMatchInputSuppressed = false;

		if (IsValid(PC))
		{
			PC->SetIgnoreLookInput(false);
			PC->SetIgnoreMoveInput(false);
		}

		PopPopupInputMode(PC);
	}
	else
	{
		ApplyGameInputMode(PC);

		// HUD가 늦게 스폰되면(중간 합류 같은 경우) BeginPlay가 PC->MyHUD 세팅보다 먼저 돌아버린다.
		// 그 사이엔 PC->GetHUD()가 비어 있으니 여기선 값을 못 넣는다.
		bCenterMouse = true;
	}

	if (!IsValid(PlayerHUDWidgetClass))
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("PlayerHUDWidgetClass is not set"));
		return;
	}

	PlayerHUDWidget = CreateWidget<UTRPlayerHUDWidget>(PC, PlayerHUDWidgetClass);
	if (IsValid(PlayerHUDWidget))
	{
		PlayerHUDWidget->AddToViewport();
	}
}

static constexpr int32 ControlConsoleZOrder = 100;

void ATRPlayerHUD::ToggleControlConsole()
{
	if (IsControlConsoleOpen())
	{
		CloseControlConsole();
		return;
	}

	if (!IsValid(ControlConsoleWidgetClass))
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("ControlConsoleWidgetClass is not set"));
		return;
	}

	ControlConsoleWidget = CreateWidget<UTRControlConsoleWidget>(GetOwningPlayerController(), ControlConsoleWidgetClass);
	if (IsValid(ControlConsoleWidget))
	{
		ControlConsoleWidget->AddToViewport(ControlConsoleZOrder);
	}
}

void ATRPlayerHUD::CloseControlConsole()
{
	if (!IsControlConsoleOpen())
	{
		return;
	}

	// 커서와 입력 원복은 위젯의 NativeDestruct가 한다.
	ControlConsoleWidget->RemoveFromParent();
	ControlConsoleWidget = nullptr;
}

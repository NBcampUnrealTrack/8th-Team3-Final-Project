#include "UI/Session/TRSessionMenuWidget.h"

#include "Session/TRNetLog.h"
#include "Session/TRSessionFlow.h"
#include "Session/Interface_TRSession.h"
#include "UI/Session/TRSessionMenuController.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UTRSessionMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 포커스를 먼저 열어 둔다. 아래 로비 갱신이 포커스를 잡으려 하기 때문이다.
	SetIsFocusable(true);

	// 판 시작 자동 닫기 판단에 쓴다. 갱신을 돌리기 전에 정해야 한다.
	if (IInterface_TRSession* Session = GetSession())
	{
		bOpenedDuringMatch = Session->GetLobbyStatus().bMatchStarted;
	}

	BindButtons();
	BindSessionEvents();
	RefreshStatusTexts();
	RefreshLobbyControls();

	// 준비 상황은 복제로 바뀌므로 알림이 없다. 주기적으로 다시 읽는다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(LobbyRefreshTimer, this,
			&UTRSessionMenuWidget::RefreshLobbyControls, 0.25f, /*bLoop=*/true);
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		bPrevShowMouseCursor = PC->bShowMouseCursor;

		//	GameAndUI 로 둔다. UIOnly 면 컨트롤러가 키를 못 받아, 트래블 뒤 포커스가
		//	위젯에 안 들어왔을 때 여는 키로 닫을 방법이 사라진다.
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);

		//	대신 시점·이동만 막는다. 안 그러면 마우스가 카메라까지 돌려 화면이 휙휙 튄다.
		//	카운터 방식이라 닫을 때 정확히 한 번 되돌린다.
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
		bInputSuppressed = true;
	}
}

FReply UTRSessionMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Pressed = InKeyEvent.GetKey();

	FKey ToggleKey = EKeys::F9;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UTRSessionMenuController* Controller = GI->GetSubsystem<UTRSessionMenuController>())
		{
			ToggleKey = Controller->GetDebugToggleKey();
		}
	}

	if (Pressed == EKeys::Escape || (ToggleKey.IsValid() && Pressed == ToggleKey))
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UTRSessionMenuWidget::NativeDestruct()
{
	UnbindSessionEvents();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LobbyRefreshTimer);
	}

	// 게임 화면으로 돌려준다. 커서는 열기 전 상태를 그대로 되돌린다.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (bInputSuppressed)
		{
			PC->SetIgnoreLookInput(false);
			PC->SetIgnoreMoveInput(false);
			bInputSuppressed = false;
		}

		// 기본 캡처 모드(CapturePermanently_IncludingInitialMouseDown)로 맞춘다.
		// 이걸 빼면 닫은 뒤 첫 클릭이 씹혀 마우스가 튀는 것처럼 보인다.
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(true);

		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(bPrevShowMouseCursor);
	}

	Super::NativeDestruct();
}

IInterface_TRSession* UTRSessionMenuWidget::GetSession() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTRSessionFlow>() : nullptr;
}

void UTRSessionMenuWidget::BindButtons()
{
	// WBP 가 만들지 않은 버튼은 그냥 없는 기능이 된다. 있는 것만 잇는다.
	if (Button_Host)
	{
		Button_Host->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleHostClicked);
	}

	if (Button_JoinByCode)
	{
		Button_JoinByCode->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleJoinByCodeClicked);
	}

	if (Button_Refresh)
	{
		Button_Refresh->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleRefreshClicked);
	}

	if (Button_Invite)
	{
		Button_Invite->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleInviteClicked);
	}

	if (Button_Leave)
	{
		Button_Leave->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleLeaveClicked);
	}

	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleCloseClicked);
	}

	if (Button_Ready)
	{
		Button_Ready->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleReadyClicked);
	}

	if (Button_Start)
	{
		Button_Start->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleStartClicked);
	}

	if (Button_JoinSlot0)
	{
		Button_JoinSlot0->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleJoinSlot0);
	}

	if (Button_JoinSlot1)
	{
		Button_JoinSlot1->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleJoinSlot1);
	}

	if (Button_JoinSlot2)
	{
		Button_JoinSlot2->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleJoinSlot2);
	}

	if (Button_JoinSlot3)
	{
		Button_JoinSlot3->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleJoinSlot3);
	}

	// 검색 전에는 빈 자리를 보여줄 이유가 없다.
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		ApplySlot(SlotIndex, nullptr);
	}
}

void UTRSessionMenuWidget::BindSessionEvents()
{
	IInterface_TRSession* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	Session->GetSessionPhaseChangedEvent().AddUniqueDynamic(this, &UTRSessionMenuWidget::HandlePhaseChanged);
	Session->GetSessionListReadyEvent().AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleListReady);
}

void UTRSessionMenuWidget::UnbindSessionEvents()
{
	if (IInterface_TRSession* Session = GetSession())
	{
		Session->GetSessionPhaseChangedEvent().RemoveDynamic(this, &UTRSessionMenuWidget::HandlePhaseChanged);
		Session->GetSessionListReadyEvent().RemoveDynamic(this, &UTRSessionMenuWidget::HandleListReady);
	}
}

// ─────────────────────────────────────────────────────────────
// 버튼
// ─────────────────────────────────────────────────────────────

void UTRSessionMenuWidget::HandleHostClicked()
{
	IInterface_TRSession* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	const FString RoomName = Input_RoomName ? Input_RoomName->GetText().ToString() : FString();
	Session->HostCreateRoom(RoomName);
}

void UTRSessionMenuWidget::HandleJoinByCodeClicked()
{
	IInterface_TRSession* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	// 빈 코드는 사용자 입력 오류다. 플로우가 안내 문구를 돌려준다.
	const FString Code = Input_JoinCode ? Input_JoinCode->GetText().ToString() : FString();
	Session->JoinRoomByCode(Code);
}

void UTRSessionMenuWidget::HandleRefreshClicked()
{
	IInterface_TRSession* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	Session->RefreshSessionList(FTRSessionSearchFilter());
}

void UTRSessionMenuWidget::HandleInviteClicked()
{
	if (IInterface_TRSession* Session = GetSession())
	{
		Session->OpenInviteOverlay();
	}
}

void UTRSessionMenuWidget::HandleLeaveClicked()
{
	if (IInterface_TRSession* Session = GetSession())
	{
		Session->LeaveSession();
	}

	RefreshStatusTexts();
}

void UTRSessionMenuWidget::HandleCloseClicked()
{
	RequestClose();
}

void UTRSessionMenuWidget::RequestClose()
{
	// 위젯이 스스로 사라지지 않는다. 여닫는 판단은 컨트롤러 몫이다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTRSessionMenuController* Controller = GI->GetSubsystem<UTRSessionMenuController>())
		{
			Controller->Close();
		}
	}

	// 컨트롤러가 이미 참조를 놓친 위젯은 여기서 직접 내린다.
	// 안 그러면 화면에 남아 입력 억제를 쥔 채 F9 와 마우스를 막는다.
	if (IsInViewport())
	{
		RemoveFromParent();
	}
}

void UTRSessionMenuWidget::JoinByIndex(int32 ResultIndex)
{
	if (IInterface_TRSession* Session = GetSession())
	{
		Session->JoinSessionByIndex(ResultIndex);
	}
}

// ─────────────────────────────────────────────────────────────
// 통지
// ─────────────────────────────────────────────────────────────

void UTRSessionMenuWidget::HandlePhaseChanged(ETRSessionPhase Phase, const FString& Message)
{
	SetStatus(Message);
	RefreshStatusTexts();

	OnSessionPhaseChanged(Phase, Message);
}

void UTRSessionMenuWidget::HandleListReady(const TArray<FTRSessionSearchResult>& Results)
{
	// 고정 자리를 위에서부터 채운다. 자리보다 결과가 많으면 나머지는 안 보인다.
	SlotResultIndices.Reset();

	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		const FTRSessionSearchResult* Result = Results.IsValidIndex(SlotIndex) ? &Results[SlotIndex] : nullptr;

		ApplySlot(SlotIndex, Result);
		SlotResultIndices.Add(Result ? Result->ResultIndex : INDEX_NONE);
	}

	if (Results.Num() > 4)
	{
		UE_LOG(LogTRNet, Log, TEXT("[SessionMenu] 방 %d개 중 4개만 표시한다"), Results.Num());
	}

	// 행 위젯을 직접 그리는 WBP 를 위해 원본도 그대로 넘긴다.
	OnSessionListReady(Results);
}

void UTRSessionMenuWidget::ApplySlot(int32 SlotIndex, const FTRSessionSearchResult* Result)
{
	UTextBlock* Label = nullptr;
	UButton* JoinButton = nullptr;

	switch (SlotIndex)
	{
	case 0:
		Label = Text_Slot0;
		JoinButton = Button_JoinSlot0;
		break;

	case 1:
		Label = Text_Slot1;
		JoinButton = Button_JoinSlot1;
		break;

	case 2:
		Label = Text_Slot2;
		JoinButton = Button_JoinSlot2;
		break;

	case 3:
		Label = Text_Slot3;
		JoinButton = Button_JoinSlot3;
		break;

	default:
		return;
	}

	const ESlateVisibility RowVisibility = Result ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (Label)
	{
		Label->SetVisibility(Result ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Result)
		{
			Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %d/%d  %dms"),
				*Result->RoomName, Result->CurrentPlayers, Result->MaxPlayers, Result->PingMs)));
		}
	}

	if (JoinButton)
	{
		JoinButton->SetVisibility(RowVisibility);

		// 정원이 찼거나 난입이 막힌 방은 눌러도 소용없다.
		JoinButton->SetIsEnabled(Result && Result->bCanJoin);
	}
}

void UTRSessionMenuWidget::HandleJoinSlot0()
{
	JoinBySlot(0);
}

void UTRSessionMenuWidget::HandleJoinSlot1()
{
	JoinBySlot(1);
}

void UTRSessionMenuWidget::HandleJoinSlot2()
{
	JoinBySlot(2);
}

void UTRSessionMenuWidget::HandleJoinSlot3()
{
	JoinBySlot(3);
}

void UTRSessionMenuWidget::JoinBySlot(int32 SlotIndex)
{
	if (!SlotResultIndices.IsValidIndex(SlotIndex) || SlotResultIndices[SlotIndex] == INDEX_NONE)
	{
		return;   // 빈 자리다. 목록을 갱신하기 전이거나 결과가 그만큼 없다.
	}

	JoinByIndex(SlotResultIndices[SlotIndex]);
}

void UTRSessionMenuWidget::RefreshStatusTexts()
{
	IInterface_TRSession* Session = GetSession();
	if (!Session)
	{
		return;
	}

	if (Text_RoomCode)
	{
		const FString Code = Session->GetRoomCode();

		Text_RoomCode->SetVisibility(Code.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (!Code.IsEmpty())
		{
			Text_RoomCode->SetText(FText::Format(NSLOCTEXT("TR", "RoomCode", "방 코드 {0}"), FText::FromString(Code)));
		}
	}

	if (Text_Lobby)
	{
		const FTRLobbyStatus Status = Session->GetLobbyStatus();
		Text_Lobby->SetText(FText::Format(NSLOCTEXT("TR", "LobbyCount", "접속 {0}명"), Status.CurrentPlayers));
	}
}

void UTRSessionMenuWidget::HandleReadyClicked()
{
	UTRSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTRSessionFlow>() : nullptr;
	if (Flow == nullptr)
	{
		return;
	}

	Flow->SetReady(!Flow->GetLobbyStatus().bLocalReady);
}

void UTRSessionMenuWidget::HandleStartClicked()
{
	if (UTRSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTRSessionFlow>() : nullptr)
	{
		Flow->StartMatch();
	}
}

void UTRSessionMenuWidget::RefreshLobbyControls()
{
	IInterface_TRSession* Session = GetSession();
	if (Session == nullptr)
	{
		return;
	}

	const FTRLobbyStatus Status = Session->GetLobbyStatus();
	const bool bHost = Session->IsHost();

	// 값이 그대로면 아무것도 건드리지 않는다. 포커스를 가진 버튼을 다시 껐다 켜면
	// Slate 가 키보드 포커스를 놓아 버려서 F9·ESC 가 먹통이 된다.
	if (bHost == bLastHost
		&& Status.bCanStart == bLastCanStart
		&& Status.bMatchStarted == bLastMatchStarted
		&& Status.bLocalReady == bLastLocalReady
		&& Status.ReadyPlayers == LastReadyPlayers
		&& Status.RequiredPlayers == LastRequiredPlayers)
	{
		return;
	}

	bLastHost = bHost;
	bLastCanStart = Status.bCanStart;
	bLastMatchStarted = Status.bMatchStarted;
	bLastLocalReady = Status.bLocalReady;
	LastReadyPlayers = Status.ReadyPlayers;
	LastRequiredPlayers = Status.RequiredPlayers;

	if (Text_Ready)
	{
		Text_Ready->SetText(FText::Format(NSLOCTEXT("TR", "ReadyCount", "준비 {0} / {1}"),
			Status.ReadyPlayers, Status.RequiredPlayers));
	}

	// 호스트는 준비를 누르지 않는다. 시작 버튼이 그 역할을 한다.
	// 판이 돌기 시작하면 둘 다 할 일이 없으므로 감춘다.
	if (Button_Ready)
	{
		const bool bShow = !bHost && !Status.bMatchStarted;
		Button_Ready->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 누르면 무엇이 되는지를 적는다. 준비한 사람에게는 취소 버튼으로 보인다.
	if (Button_Ready_Text)
	{
		Button_Ready_Text->SetText(Status.bLocalReady
			? NSLOCTEXT("TR", "CancelReady", "준비 취소")
			: NSLOCTEXT("TR", "Ready", "준비"));
	}

	if (Button_Start)
	{
		const bool bShow = bHost && !Status.bMatchStarted;
		Button_Start->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_Start->SetIsEnabled(Status.bCanStart);
	}

	// 위 조작으로 포커스가 떨어졌으면 되돌린다. 닫는 키는 이 위젯이 직접 받는다.
	if (IsFocusable() && !HasAnyUserFocus() && !HasFocusedDescendants())
	{
		SetKeyboardFocus();
	}

	// 열어 둔 사이에 판이 시작되면 로비를 붙잡고 있을 이유가 없다.
	// 판이 도는 중에 직접 연 메뉴는 사용자가 닫을 때까지 둔다.
	if (Status.bMatchStarted && !bOpenedDuringMatch && !bClosePending)
	{
		// 세션 이벤트 콜백으로도 이 함수가 불린다. 표시가 없으면 다음 틱 타이머가 겹쳐 걸린다.
		bClosePending = true;

		UWorld* World = GetWorld();
		if (World == nullptr)
		{
			// 월드가 없으면 다음 틱을 기약할 수 없다. 즉시 닫는다.
			RequestClose();
			return;
		}

		// 닫기 전에 타이머부터 끊는다. 위젯이 안 지워지는 경우 이 함수가 계속 돌면서
		// 입력 억제를 쥔 채 남는다.
		World->GetTimerManager().ClearTimer(LobbyRefreshTimer);

		// 뷰포트에 붙는 중일 수 있어 닫기는 다음 틱으로 미룬다.
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				RequestClose();
			}));
	}
}

void UTRSessionMenuWidget::SetStatus(const FString& Message)
{
	if (Text_Status && !Message.IsEmpty())
	{
		Text_Status->SetText(FText::FromString(Message));
	}
}

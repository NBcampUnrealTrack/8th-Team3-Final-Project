// TRMainMenuWidget.cpp

#include "UI/TRMainMenuWidget.h"

#include "CommonButtonBase.h"
#include "Components/EditableTextBox.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Session/Interface_TRSession.h"
#include "Session/TRNetLog.h"
#include "Session/TRSessionFlow.h"
#include "TimerManager.h"
#include "UI/TRMenuButton.h"
#include "UI/Settings/TRSettingsWidget.h"

void UTRMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(Btn_FindGame))
	{
		Btn_FindGame->OnClicked().RemoveAll(this);
		Btn_FindGame->OnClicked().AddUObject(this, &UTRMainMenuWidget::HandleFindGameClicked);
	}

	SettingsButton = GetWidgetFromName(TEXT("Btn_Settings"));

	if (UCommonButtonBase* SettingsAsButton = Cast<UCommonButtonBase>(SettingsButton))
	{
		SettingsAsButton->OnClicked().RemoveAll(this);
		SettingsAsButton->OnClicked().AddUObject(this, &UTRMainMenuWidget::HandleSettingsClicked);
	}

	ShowOnlyPanel(nullptr);

	BindSessionButtons();
	BindSessionEvents();
	ApplyPhaseLocks(ETRSessionPhase::Idle);
	RefreshSessionPresence();
	RefreshStatusTexts();
	RefreshLobbyControls();

	// 준비 상황은 복제로 바뀌므로 알림이 없다. 주기적으로 다시 읽는다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(LobbyRefreshTimer, this,
			&UTRMainMenuWidget::RefreshLobbyControls, 0.25f, /*bLoop=*/true);
	}
}

void UTRMainMenuWidget::NativeDestruct()
{
	CloseSettings();

	UnbindSessionEvents();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LobbyRefreshTimer);
	}

	Super::NativeDestruct();
}

void UTRMainMenuWidget::ShowOnlyPanel(UWidget* PanelToShow)
{
	for (UWidget* Panel : { Border_Session.Get(), Border_Find.Get() })
	{
		if (!IsValid(Panel))
		{
			continue;
		}

		// 안쪽 버튼이 눌려야 하므로 펼칠 때도 자기만 입력을 비운다.
		Panel->SetVisibility(Panel == PanelToShow
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	bFindPanelOpen = PanelToShow != nullptr && PanelToShow == Border_Find;

	RefreshLobbyControls();
}

bool UTRMainMenuWidget::IsInSession() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() != NM_Standalone;
}

bool UTRMainMenuWidget::HasHostedRoom() const
{
	const IInterface_TRSession* Session = GetSession();
	return Session != nullptr && !Session->GetRoomCode().IsEmpty();
}

void UTRMainMenuWidget::RefreshSessionPresence()
{
	const bool bInSession = IsInSession();
	if (LastInSession.IsSet() && LastInSession.GetValue() == bInSession)
	{
		return;
	}

	LastInSession = bInSession;

	if (IsValid(Btn_Leave))
	{
		Btn_Leave->SetVisibility(bInSession ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UTRMainMenuWidget::HandleFindGameClicked()
{
	// 빈 목록으로 열리지 않게 검색을 한 번 걸어 준다.
	ShowOnlyPanel(Border_Find);
	HandleRefreshClicked();
}

void UTRMainMenuWidget::HandleSettingsClicked()
{
	if (SettingsPopup)
	{
		return;
	}

	TSubclassOf<UTRSettingsWidget> Class = SettingsWidgetClass.LoadSynchronous();
	if (!ensureMsgf(Class, TEXT("WBP 의 SettingsWidgetClass 가 비었거나 로드에 실패했습니다: %s"),
		*SettingsWidgetClass.ToString()))
	{
		return;
	}

	SettingsPopup = CreateWidget<UTRSettingsWidget>(GetOwningPlayer(), Class);
	if (!ensure(SettingsPopup))
	{
		return;
	}

	// 뒤로 버튼은 위젯을 비활성화만 한다. 뷰포트에서 내리는 것은 띄운 쪽 몫이다.
	// UTRSettingsWidget 은 bAutoActivate 가 참이라 AddToViewport 안에서 곧바로
	// 활성화된다. 먼저 걸어 두지 않으면 그 사이의 비활성화를 놓쳐 팝업이 남는다.
	SettingsPopup->OnDeactivated().AddWeakLambda(this, [this]()
	{
		CloseSettings();
	});

	SettingsPopup->AddToViewport(200);

	// bAutoActivate 가 꺼진 WBP 로 갈아끼워도 열리게 해 둔다. 이미 활성이면 무시된다.
	SettingsPopup->ActivateWidget();
}

void UTRMainMenuWidget::CloseSettings()
{
	if (!SettingsPopup)
	{
		return;
	}

	SettingsPopup->RemoveFromParent();
	SettingsPopup = nullptr;
}

IInterface_TRSession* UTRMainMenuWidget::GetSession() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UTRSessionFlow>() : nullptr;
}

void UTRMainMenuWidget::BindSessionButtons()
{
	// 버튼에 적히는 글자는 WBP 가 들고 있다. 여기서는 핸들러만 잇는다.
	const TPair<UTRMenuButton*, void (UTRMainMenuWidget::*)()> Bindings[] =
	{
		{ Btn_Host,       &UTRMainMenuWidget::HandleHostClicked },
		{ Btn_RenameRoom, &UTRMainMenuWidget::HandleRenameRoomClicked },
		{ Btn_JoinByCode, &UTRMainMenuWidget::HandleJoinByCodeClicked },
		{ Btn_Refresh,    &UTRMainMenuWidget::HandleRefreshClicked },
		{ Btn_Invite,     &UTRMainMenuWidget::HandleInviteClicked },
		{ Btn_Leave,      &UTRMainMenuWidget::HandleLeaveClicked },
		{ Btn_Ready,      &UTRMainMenuWidget::HandleReadyClicked },
		{ Btn_Start,      &UTRMainMenuWidget::HandleStartClicked },
		{ Btn_Quit,       &UTRMainMenuWidget::HandleQuitClicked },
	};

	// WBP 가 만들지 않은 버튼은 그냥 없는 기능이 된다. 있는 것만 잇는다.
	for (const TPair<UTRMenuButton*, void (UTRMainMenuWidget::*)()>& Binding : Bindings)
	{
		if (IsValid(Binding.Key))
		{
			Binding.Key->OnClicked().RemoveAll(this);
			Binding.Key->OnClicked().AddUObject(this, Binding.Value);
		}
	}

	// 자리마다 인덱스가 달라 람다로 잇는다.
	UTRMenuButton* const JoinButtons[SlotCount] = { Btn_JoinSlot0, Btn_JoinSlot1, Btn_JoinSlot2, Btn_JoinSlot3 };
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		if (IsValid(JoinButtons[SlotIndex]))
		{
			JoinButtons[SlotIndex]->OnClicked().RemoveAll(this);
			JoinButtons[SlotIndex]->OnClicked().AddWeakLambda(this, [this, SlotIndex]()
			{
				JoinBySlot(SlotIndex);
			});
		}

		// 검색 전에는 빈 자리를 보여줄 이유가 없다.
		ApplySlot(SlotIndex, nullptr);
	}
}

void UTRMainMenuWidget::BindSessionEvents()
{
	IInterface_TRSession* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	Session->GetSessionPhaseChangedEvent().AddUniqueDynamic(this, &UTRMainMenuWidget::HandlePhaseChanged);
	Session->GetSessionListReadyEvent().AddUniqueDynamic(this, &UTRMainMenuWidget::HandleListReady);
}

void UTRMainMenuWidget::UnbindSessionEvents()
{
	if (IInterface_TRSession* Session = GetSession())
	{
		Session->GetSessionPhaseChangedEvent().RemoveDynamic(this, &UTRMainMenuWidget::HandlePhaseChanged);
		Session->GetSessionListReadyEvent().RemoveDynamic(this, &UTRMainMenuWidget::HandleListReady);
	}
}

// ─────────────────────────────────────────────────────────────
// 버튼
// ─────────────────────────────────────────────────────────────

void UTRMainMenuWidget::HandleHostClicked()
{
	// 생성 상태를 볼 수 있어야 하므로 요청보다 패널을 먼저 펼친다.
	ShowOnlyPanel(Border_Session);

	// 세션에 들어가 있으면 패널만 펼치고 끝낸다. HostCreateRoom 은 기존 세션을 파기하므로
	// 호스트는 손님을 끊고, 남의 방에 참가한 클라이언트는 자기가 그 방에서 튕겨 나간다.
	// HasHostedRoom 은 호스트만 참이라 클라이언트를 못 막으므로 넷모드도 함께 본다.
	if (IsInSession() || HasHostedRoom())
	{
		return;
	}

	IInterface_TRSession* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	const FString RoomName = Input_RoomName ? Input_RoomName->GetText().ToString() : FString();
	Session->HostCreateRoom(RoomName);
}

void UTRMainMenuWidget::HandleRenameRoomClicked()
{
	IInterface_TRSession* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	// 세션 이름만 갱신하는 경로가 없어 방을 다시 만드는 것이 전부다.
	const FString RoomName = Input_RoomName ? Input_RoomName->GetText().ToString() : FString();
	if (RoomName.TrimStartAndEnd().IsEmpty())
	{
		SetStatus(TEXT("방 이름을 입력하세요"));
		return;
	}

	Session->HostCreateRoom(RoomName);
}

void UTRMainMenuWidget::HandleJoinByCodeClicked()
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

void UTRMainMenuWidget::HandleRefreshClicked()
{
	IInterface_TRSession* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	Session->RefreshSessionList(FTRSessionSearchFilter());
}

void UTRMainMenuWidget::HandleInviteClicked()
{
	if (IInterface_TRSession* Session = GetSession())
	{
		Session->OpenInviteOverlay();
	}
}

void UTRMainMenuWidget::HandleLeaveClicked()
{
	if (IInterface_TRSession* Session = GetSession())
	{
		Session->LeaveSession();
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (World->GetNetMode() != NM_Standalone)
	{
		// PIE 에서는 패키지 이름에 UEDPIE_N_ 이 붙으므로 벗겨야 열린다.
		UGameplayStatics::OpenLevel(World, FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())));
		return;
	}

	RefreshStatusTexts();
}

void UTRMainMenuWidget::HandleReadyClicked()
{
	UTRSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTRSessionFlow>() : nullptr;
	if (Flow == nullptr)
	{
		return;
	}

	Flow->SetReady(!Flow->GetLobbyStatus().bLocalReady);
}

void UTRMainMenuWidget::HandleStartClicked()
{
	if (UTRSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTRSessionFlow>() : nullptr)
	{
		Flow->StartMatch();
	}
}

void UTRMainMenuWidget::HandleQuitClicked()
{
	// 세션을 남긴 채 끝내면 다음 실행에서 유령 방이 남는다.
	if (IInterface_TRSession* Session = GetSession())
	{
		Session->LeaveSession();
	}

	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

// ─────────────────────────────────────────────────────────────
// 통지
// ─────────────────────────────────────────────────────────────

void UTRMainMenuWidget::HandlePhaseChanged(ETRSessionPhase Phase, const FString& Message)
{
	SetStatus(Message);
	RefreshStatusTexts();
	ApplyPhaseLocks(Phase);
}

void UTRMainMenuWidget::ApplyPhaseLocks(ETRSessionPhase Phase)
{
	// 생성이 끝나기 전에 또 누르면 세션을 파기하고 다시 만든다. 그 사이에는 잠근다.
	const bool bCreateIdle = Phase != ETRSessionPhase::Creating;

	if (IsValid(Btn_Host))
	{
		Btn_Host->SetLocked(!bCreateIdle);
	}

	// 방 이름 변경도 결국 생성 요청이다.
	if (IsValid(Btn_RenameRoom))
	{
		Btn_RenameRoom->SetLocked(!bCreateIdle);
	}

	// 검색은 코드 참가와 목록 갱신이 공유한다. 하나가 도는 동안 둘 다 막는다.
	const bool bSearchIdle = Phase != ETRSessionPhase::Searching && Phase != ETRSessionPhase::Joining;

	if (IsValid(Btn_JoinByCode))
	{
		Btn_JoinByCode->SetLocked(!bSearchIdle);
	}

	if (IsValid(Btn_Refresh))
	{
		Btn_Refresh->SetLocked(!bSearchIdle);
	}
}

void UTRMainMenuWidget::HandleListReady(const TArray<FTRSessionSearchResult>& Results)
{
	// 고정 자리를 위에서부터 채운다. 자리보다 결과가 많으면 나머지는 안 보인다.
	SlotResultIndices.Reset();

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		const FTRSessionSearchResult* Result = Results.IsValidIndex(SlotIndex) ? &Results[SlotIndex] : nullptr;

		ApplySlot(SlotIndex, Result);
		SlotResultIndices.Add(Result ? Result->ResultIndex : INDEX_NONE);
	}

	if (Results.Num() > SlotCount)
	{
		UE_LOG(LogTRNet, Log, TEXT("[MainMenu] 방 %d개 중 %d개만 표시한다"), Results.Num(), SlotCount);
	}
}

void UTRMainMenuWidget::ApplySlot(int32 SlotIndex, const FTRSessionSearchResult* Result)
{
	UTextBlock* const Labels[SlotCount] = { Txt_Slot0, Txt_Slot1, Txt_Slot2, Txt_Slot3 };
	UTRMenuButton* const JoinButtons[SlotCount] = { Btn_JoinSlot0, Btn_JoinSlot1, Btn_JoinSlot2, Btn_JoinSlot3 };

	if (SlotIndex < 0 || SlotIndex >= SlotCount)
	{
		return;
	}

	const ESlateVisibility RowVisibility = Result ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (UTextBlock* Label = Labels[SlotIndex])
	{
		Label->SetVisibility(Result ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Result)
		{
			Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %d/%d  %dms"),
				*Result->RoomName, Result->CurrentPlayers, Result->MaxPlayers, Result->PingMs)));
		}
	}

	if (UTRMenuButton* JoinButton = JoinButtons[SlotIndex])
	{
		JoinButton->SetVisibility(RowVisibility);

		// 정원이 찼거나 난입이 막힌 방은 눌러도 소용없다.
		JoinButton->SetLocked(!(Result && Result->bCanJoin));
	}
}

void UTRMainMenuWidget::JoinBySlot(int32 SlotIndex)
{
	if (!SlotResultIndices.IsValidIndex(SlotIndex) || SlotResultIndices[SlotIndex] == INDEX_NONE)
	{
		return;   // 빈 자리다. 목록을 갱신하기 전이거나 결과가 그만큼 없다.
	}

	if (IInterface_TRSession* Session = GetSession())
	{
		Session->JoinSessionByIndex(SlotResultIndices[SlotIndex]);
	}
}

void UTRMainMenuWidget::RefreshStatusTexts()
{
	IInterface_TRSession* Session = GetSession();
	if (!Session)
	{
		return;
	}

	if (Txt_RoomCode)
	{
		const FString Code = Session->GetRoomCode();

		Txt_RoomCode->SetVisibility(Code.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (!Code.IsEmpty())
		{
			Txt_RoomCode->SetText(FText::Format(NSLOCTEXT("TR", "RoomCode", "방 코드 {0}"), FText::FromString(Code)));
		}
	}

	if (Txt_Lobby)
	{
		const FTRLobbyStatus Status = Session->GetLobbyStatus();
		Txt_Lobby->SetText(FText::Format(NSLOCTEXT("TR", "LobbyCount", "접속 {0}명"), Status.CurrentPlayers));
	}
}

void UTRMainMenuWidget::RefreshLobbyControls()
{
	// 세션에 들고 나는 것은 알림이 따로 없다. 여기서 같이 확인한다.
	RefreshSessionPresence();

	IInterface_TRSession* Session = GetSession();
	if (Session == nullptr)
	{
		return;
	}

	const FTRLobbyStatus Status = Session->GetLobbyStatus();
	const bool bHost = Session->IsHost();
	const bool bInSession = IsInSession();
	const bool bHasRoom = HasHostedRoom();

	// 값이 그대로면 아무것도 건드리지 않는다. 포커스를 가진 버튼을 다시 껐다 켜면
	// Slate 가 키보드 포커스를 놓아 버린다.
	if (bHost == bLastHost
		&& Status.bCanStart == bLastCanStart
		&& Status.bMatchStarted == bLastMatchStarted
		&& Status.bLocalReady == bLastLocalReady
		&& Status.ReadyPlayers == LastReadyPlayers
		&& Status.RequiredPlayers == LastRequiredPlayers
		&& bInSession == bLastInSessionControls
		&& bHasRoom == bLastHasHostedRoom
		&& bFindPanelOpen == bLastFindPanelOpen)
	{
		return;
	}

	bLastHost = bHost;
	bLastCanStart = Status.bCanStart;
	bLastMatchStarted = Status.bMatchStarted;
	bLastLocalReady = Status.bLocalReady;
	LastReadyPlayers = Status.ReadyPlayers;
	LastRequiredPlayers = Status.RequiredPlayers;
	bLastInSessionControls = bInSession;
	bLastHasHostedRoom = bHasRoom;
	bLastFindPanelOpen = bFindPanelOpen;

	if (Txt_Ready)
	{
		Txt_Ready->SetText(FText::Format(NSLOCTEXT("TR", "ReadyCount", "준비 {0} / {1}"),
			Status.ReadyPlayers, Status.RequiredPlayers));
	}

	// HandleHostClicked 와 같은 기준을 봐야 글자와 실제 동작이 어긋나지 않는다.
	if (Btn_Host)
	{
		Btn_Host->SetLabelText(bInSession || bHasRoom
			? NSLOCTEXT("TR", "MainMenuViewRoom", "방 보기")
			: NSLOCTEXT("TR", "MainMenuHostRoom", "방 만들기"));
	}

	// 호스트는 준비를 누르지 않는다. 시작 버튼이 그 역할을 한다.
	if (Btn_Ready)
	{
		const bool bShow = bInSession && !bHost && !Status.bMatchStarted;
		Btn_Ready->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

		// 누르면 무엇이 되는지를 적는다. 준비한 사람에게는 취소 버튼으로 보인다.
		Btn_Ready->SetLabelText(Status.bLocalReady
			? NSLOCTEXT("TR", "MainMenuCancelReady", "준비 취소")
			: NSLOCTEXT("TR", "MainMenuReady", "준비"));
	}

	if (Btn_Start)
	{
		const bool bShow = bHasRoom && bHost && !Status.bMatchStarted && !bFindPanelOpen;
		Btn_Start->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_Start->SetLocked(!Status.bCanStart);
	}
}

void UTRMainMenuWidget::SetStatus(const FString& Message)
{
	if (Txt_Status && !Message.IsEmpty())
	{
		Txt_Status->SetText(FText::FromString(Message));
	}
}

// TRMainMenuWidget.cpp

#include "UI/TRMainMenuWidget.h"

#include "CommonButtonBase.h"
#include "Components/EditableTextBox.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Session/Interface_TRSession.h"
#include "Session/TRNetLog.h"
#include "Session/TRSessionFlow.h"
#include "TimerManager.h"
#include "UI/TRMenuButton.h"

void UTRMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(Btn_FindGame))
	{
		Btn_FindGame->OnClicked().RemoveAll(this);
		Btn_FindGame->OnClicked().AddUObject(this, &UTRMainMenuWidget::HandleFindGameClicked);
	}

	// 세션 패널은 Btn_FindGame 을 누를 때까지 접어 둔다.
	if (IsValid(Border_Session))
	{
		Border_Session->SetVisibility(ESlateVisibility::Collapsed);
	}

	BindSessionButtons();
	BindSessionEvents();
	ApplyPhaseLocks(ETRSessionPhase::Idle);
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
	UnbindSessionEvents();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LobbyRefreshTimer);
	}

	Super::NativeDestruct();
}

void UTRMainMenuWidget::HandleFindGameClicked()
{
	// 세션 패널은 이 버튼으로만 열린다. 안쪽 버튼이 눌려야 하므로 자기만 입력을 비운다.
	if (IsValid(Border_Session))
	{
		Border_Session->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
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
	IInterface_TRSession* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	const FString RoomName = Input_RoomName ? Input_RoomName->GetText().ToString() : FString();
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

	// 메인 메뉴는 이미 나갈 곳이라 레벨을 다시 열지 않는다. 표시만 맞춘다.
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
	if (IsValid(Btn_Host))
	{
		Btn_Host->SetIsEnabled(Phase != ETRSessionPhase::Creating);
	}

	// 검색은 코드 참가와 목록 갱신이 공유한다. 하나가 도는 동안 둘 다 막는다.
	const bool bSearchIdle = Phase != ETRSessionPhase::Searching && Phase != ETRSessionPhase::Joining;

	if (IsValid(Btn_JoinByCode))
	{
		Btn_JoinByCode->SetIsEnabled(bSearchIdle);
	}

	if (IsValid(Btn_Refresh))
	{
		Btn_Refresh->SetIsEnabled(bSearchIdle);
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
		JoinButton->SetIsEnabled(Result && Result->bCanJoin);
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
	IInterface_TRSession* Session = GetSession();
	if (Session == nullptr)
	{
		return;
	}

	const FTRLobbyStatus Status = Session->GetLobbyStatus();
	const bool bHost = Session->IsHost();

	// 값이 그대로면 아무것도 건드리지 않는다. 포커스를 가진 버튼을 다시 껐다 켜면
	// Slate 가 키보드 포커스를 놓아 버린다.
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

	if (Txt_Ready)
	{
		Txt_Ready->SetText(FText::Format(NSLOCTEXT("TR", "ReadyCount", "준비 {0} / {1}"),
			Status.ReadyPlayers, Status.RequiredPlayers));
	}

	// 호스트는 준비를 누르지 않는다. 시작 버튼이 그 역할을 한다.
	// 판이 돌기 시작하면 둘 다 할 일이 없으므로 감춘다.
	if (Btn_Ready)
	{
		const bool bShow = !bHost && !Status.bMatchStarted;
		Btn_Ready->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

		// 누르면 무엇이 되는지를 적는다. 준비한 사람에게는 취소 버튼으로 보인다.
		Btn_Ready->SetLabelText(Status.bLocalReady
			? NSLOCTEXT("TR", "MainMenuCancelReady", "준비 취소")
			: NSLOCTEXT("TR", "MainMenuReady", "준비"));
	}

	if (Btn_Start)
	{
		const bool bShow = bHost && !Status.bMatchStarted;
		Btn_Start->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_Start->SetIsEnabled(Status.bCanStart);
	}
}

void UTRMainMenuWidget::SetStatus(const FString& Message)
{
	if (Txt_Status && !Message.IsEmpty())
	{
		Txt_Status->SetText(FText::FromString(Message));
	}
}

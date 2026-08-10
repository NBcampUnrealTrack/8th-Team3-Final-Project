#include "UI/Session/TRSessionMenuWidget.h"

#include "Session/TRNetLog.h"
#include "Session/TRSessionFlow.h"
#include "Session/TRSessionInterface.h"
#include "UI/Session/TRSessionMenuController.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

void UTRSessionMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindButtons();
	BindSessionEvents();
	RefreshStatusTexts();

	// 메뉴가 떠 있는 동안은 마우스로 조작한다. 게임 입력도 살려 둔다.
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
}

void UTRSessionMenuWidget::NativeDestruct()
{
	UnbindSessionEvents();

	// 게임 화면으로 돌려준다. 커서는 게임플레이 쪽 판단이라 여기서 켜 두지 않는다.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}

	Super::NativeDestruct();
}

ITRSessionInterface* UTRSessionMenuWidget::GetSession() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTRSessionFlow>() : nullptr;
}

void UTRSessionMenuWidget::BindButtons()
{
	// WBP 가 만들지 않은 버튼은 그냥 없는 기능이 된다. 있는 것만 잇는다.
	if (Button_Host)       { Button_Host->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleHostClicked); }
	if (Button_JoinByCode) { Button_JoinByCode->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleJoinByCodeClicked); }
	if (Button_Refresh)    { Button_Refresh->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleRefreshClicked); }
	if (Button_Invite)     { Button_Invite->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleInviteClicked); }
	if (Button_Leave)      { Button_Leave->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleLeaveClicked); }
	if (Button_Close)      { Button_Close->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleCloseClicked); }

	if (Button_JoinSlot0) { Button_JoinSlot0->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleJoinSlot0); }
	if (Button_JoinSlot1) { Button_JoinSlot1->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleJoinSlot1); }
	if (Button_JoinSlot2) { Button_JoinSlot2->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleJoinSlot2); }
	if (Button_JoinSlot3) { Button_JoinSlot3->OnClicked.AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleJoinSlot3); }

	// 검색 전에는 빈 자리를 보여줄 이유가 없다.
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		ApplySlot(SlotIndex, nullptr);
	}
}

void UTRSessionMenuWidget::BindSessionEvents()
{
	ITRSessionInterface* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	Session->GetSessionPhaseChangedEvent().AddUniqueDynamic(this, &UTRSessionMenuWidget::HandlePhaseChanged);
	Session->GetSessionListReadyEvent().AddUniqueDynamic(this, &UTRSessionMenuWidget::HandleListReady);
}

void UTRSessionMenuWidget::UnbindSessionEvents()
{
	if (ITRSessionInterface* Session = GetSession())
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
	ITRSessionInterface* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	const FString RoomName = Input_RoomName ? Input_RoomName->GetText().ToString() : FString();
	Session->HostCreateRoom(RoomName);
}

void UTRSessionMenuWidget::HandleJoinByCodeClicked()
{
	ITRSessionInterface* Session = GetSession();
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
	ITRSessionInterface* Session = GetSession();
	if (!ensure(Session))
	{
		return;
	}

	Session->RefreshSessionList(FTRSessionSearchFilter());
}

void UTRSessionMenuWidget::HandleInviteClicked()
{
	if (ITRSessionInterface* Session = GetSession())
	{
		Session->OpenInviteOverlay();
	}
}

void UTRSessionMenuWidget::HandleLeaveClicked()
{
	if (ITRSessionInterface* Session = GetSession())
	{
		Session->LeaveSession();
	}

	RefreshStatusTexts();
}

void UTRSessionMenuWidget::HandleCloseClicked()
{
	// 위젯이 스스로 사라지지 않는다. 여닫는 판단은 컨트롤러 몫이다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTRSessionMenuController* Controller = GI->GetSubsystem<UTRSessionMenuController>())
		{
			Controller->Close();
		}
	}
}

void UTRSessionMenuWidget::JoinByIndex(int32 ResultIndex)
{
	if (ITRSessionInterface* Session = GetSession())
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
	case 0: Label = Text_Slot0; JoinButton = Button_JoinSlot0; break;
	case 1: Label = Text_Slot1; JoinButton = Button_JoinSlot1; break;
	case 2: Label = Text_Slot2; JoinButton = Button_JoinSlot2; break;
	case 3: Label = Text_Slot3; JoinButton = Button_JoinSlot3; break;
	default: return;
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

void UTRSessionMenuWidget::HandleJoinSlot0() { JoinBySlot(0); }
void UTRSessionMenuWidget::HandleJoinSlot1() { JoinBySlot(1); }
void UTRSessionMenuWidget::HandleJoinSlot2() { JoinBySlot(2); }
void UTRSessionMenuWidget::HandleJoinSlot3() { JoinBySlot(3); }

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
	ITRSessionInterface* Session = GetSession();
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

void UTRSessionMenuWidget::SetStatus(const FString& Message)
{
	if (Text_Status && !Message.IsEmpty())
	{
		Text_Status->SetText(FText::FromString(Message));
	}
}

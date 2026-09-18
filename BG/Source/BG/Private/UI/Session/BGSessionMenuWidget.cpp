#include "UI/Session/BGSessionMenuWidget.h"

#include "Audio/BGAudioSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Debug/BGNetLog.h"
#include "Session/BGSessionFlow.h"
#include "Session/BGSessionInterface.h"
#include "UI/Session/BGSessionMenuController.h"
#include "Lobby/BGReadyComponent.h"
#include "Core/BGGameState.h"
#include "BGGameplayTags.h"

namespace
{
	// 구분선이 화면에서 차지해야 할 픽셀 두께. DPI 스케일과 무관하게 이 값으로 떨어진다.
	//	⚠️ BGSettingsWidget.cpp 에도 같은 익명 상수가 있어 unity 빌드에서 충돌한다. 이름을 나눠 둔다.
	constexpr float SessionMenuDividerPixels = 2.f;

	FString PhaseToText(EBGSessionPhase Phase)
	{
		switch (Phase)
		{
		case EBGSessionPhase::Creating:  return TEXT("방 생성 중");
		case EBGSessionPhase::Hosting:   return TEXT("호스팅 중");
		case EBGSessionPhase::Searching: return TEXT("검색 중");
		case EBGSessionPhase::Joining:   return TEXT("접속 중");
		case EBGSessionPhase::Joined:    return TEXT("접속 완료");
		case EBGSessionPhase::Failed:    return TEXT("실패");
		default:                           return TEXT("대기");
		}
	}
}

void UBGSessionRowHandler::HandleClicked()
{
	if (Menu)
	{
		Menu->JoinByIndex(ResultIndex);
	}
}

IBGSessionInterface* UBGSessionMenuWidget::GetSession() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UBGSessionFlow>() : nullptr;
}

TSharedRef<SWidget> UBGSessionMenuWidget::RebuildWidget()
{
	if (WidgetTree)
	{
		BuildMissingWidgets();
	}
	return Super::RebuildWidget();
}

bool UBGSessionMenuWidget::HasAllWidgets() const
{
	return StatusText && RoomCodeText && RoomNameInput && HostButton && JoinCodeInput
		&& JoinCodeButton && RefreshButton && ResultList && LobbyText
		&& ReadyButton && StartGameButton && LeaveButton;
}

void UBGSessionMenuWidget::BuildMissingWidgets()
{
	if (HasAllWidgets())
	{
		return;
	}

	UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (!RootPanel)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;
		RootPanel = Canvas;
	}
	const bool bDesignerHasWidgets = RootPanel->GetChildrenCount() > 0;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FallbackPanel"));
	Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.75f));
	Panel->SetPadding(FMargin(12.f));

	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(RootPanel->AddChild(Panel)))
	{
		// 디자이너 배치를 모르니, 위젯이 이미 있으면 화면 오른쪽 끝에 붙여 충돌을 피한다.
		PanelSlot->SetAnchors(bDesignerHasWidgets ? FAnchors(1.f, 0.f) : FAnchors(0.f, 0.f));
		PanelSlot->SetAlignment(bDesignerHasWidgets ? FVector2D(1.f, 0.f) : FVector2D::ZeroVector);
		PanelSlot->SetPosition(FVector2D(bDesignerHasWidgets ? -40.f : 40.f, 40.f));
		// 위젯이 늘어나도 목록이 밀려나지 않도록 여유를 둔다.
		PanelSlot->SetSize(FVector2D(440.f, bDesignerHasWidgets ? 420.f : 780.f));
	}

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FallbackContent"));
	Panel->AddChild(Box);

	if (!StatusText)   { StatusText   = AddLabel(Box, TEXT("상태: 대기")); }
	if (!RoomCodeText) { RoomCodeText = AddLabel(Box, TEXT("방 코드: -")); }

	if (!RoomNameInput || !HostButton) { AddLabel(Box, TEXT("─ 호스트 ─")); }
	if (!RoomNameInput) { RoomNameInput = AddInput(Box, TEXT("방 이름(비우면 자동)"), TEXT("RoomNameInput")); }
	if (!HostButton)    { HostButton    = AddButton(Box, TEXT("방 만들기"), TEXT("HostButton")); }

	if (!JoinCodeInput || !JoinCodeButton || !RefreshButton) { AddLabel(Box, TEXT("─ 참여 ─")); }
	if (!JoinCodeInput)  { JoinCodeInput  = AddInput(Box, TEXT("방 코드 6자"), TEXT("JoinCodeInput")); }
	if (!JoinCodeButton) { JoinCodeButton = AddButton(Box, TEXT("코드로 참여"), TEXT("JoinCodeButton")); }
	if (!RefreshButton)  { RefreshButton  = AddButton(Box, TEXT("방 목록 새로고침"), TEXT("RefreshButton")); }

	if (!ResultList)
	{
		// 목록은 Fill 로 두면 위아래 고정 위젯이 늘어날 때 높이 0 으로 찌그러진다.
		// SizeBox 로 최소 높이를 보장해 항상 보이게 한다.
		ResultList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ResultList"));

		// 폭을 안 잡아주면 스크롤박스가 제 마음대로 커져 패널 밖으로 삐져나간다.
		USizeBox* ListBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ResultListBox"));
		ListBox->SetWidthOverride(400.f);
		ListBox->SetHeightOverride(180.f);
		ListBox->AddChild(ResultList);

		if (UVerticalBoxSlot* ListSlot = Box->AddChildToVerticalBox(ListBox))
		{
			ListSlot->SetPadding(FMargin(0.f, 6.f));
			ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	if (!LobbyText || !ReadyButton || !StartGameButton) { AddLabel(Box, TEXT("─ 진행 ─")); }
	if (!LobbyText)       { LobbyText       = AddLabel(Box, TEXT("대기 중")); }
	if (!ReadyButton)     { ReadyButton     = AddButton(Box, TEXT("준비"), TEXT("ReadyButton")); }
	if (!StartGameButton) { StartGameButton = AddButton(Box, TEXT("게임 시작 (호스트)"), TEXT("StartGameButton")); }
	if (!LeaveButton)     { LeaveButton     = AddButton(Box, TEXT("나가기"), TEXT("LeaveButton")); }
}

void UBGSessionMenuWidget::BindButtonEvents()
{
	// 디자이너가 버튼만 올렸으면 라벨 텍스트는 버튼의 첫 자식에서 찾는다.
	if (!ReadyButtonText && ReadyButton)
	{
		ReadyButtonText = Cast<UTextBlock>(ReadyButton->GetChildAt(0));
	}

	if (HostButton)      { HostButton->OnClicked.AddUniqueDynamic(this, &UBGSessionMenuWidget::HandleHostClicked); }
	if (JoinCodeButton)  { JoinCodeButton->OnClicked.AddUniqueDynamic(this, &UBGSessionMenuWidget::HandleJoinCodeClicked); }
	if (RefreshButton)   { RefreshButton->OnClicked.AddUniqueDynamic(this, &UBGSessionMenuWidget::HandleRefreshClicked); }
	if (ReadyButton)     { ReadyButton->OnClicked.AddUniqueDynamic(this, &UBGSessionMenuWidget::HandleReadyClicked); }
	if (StartGameButton) { StartGameButton->OnClicked.AddUniqueDynamic(this, &UBGSessionMenuWidget::HandleStartGameClicked); }
	if (LeaveButton)     { LeaveButton->OnClicked.AddUniqueDynamic(this, &UBGSessionMenuWidget::HandleLeaveClicked); }
}

UTextBlock* UBGSessionMenuWidget::AddLabel(UVerticalBox* Parent, const FString& Text, int32 FontSize)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(FText::FromString(Text));

	FSlateFontInfo Font = Block->GetFont();
	Font.Size = FontSize;
	Block->SetFont(Font);

	if (UVerticalBoxSlot* BoxSlot = Parent->AddChildToVerticalBox(Block))
	{
		BoxSlot->SetPadding(FMargin(0.f, 3.f));
	}
	return Block;
}

UButton* UBGSessionMenuWidget::AddButton(UVerticalBox* Parent, const FString& Label, FName WidgetName)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(Label));
	Text->SetJustification(ETextJustify::Center);
	Button->AddChild(Text);

	if (UVerticalBoxSlot* BoxSlot = Parent->AddChildToVerticalBox(Button))
	{
		BoxSlot->SetPadding(FMargin(0.f, 4.f));
	}
	return Button;
}

UEditableTextBox* UBGSessionMenuWidget::AddInput(UVerticalBox* Parent, const FString& HintText, FName WidgetName)
{
	UEditableTextBox* Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), WidgetName);
	Input->SetHintText(FText::FromString(HintText));

	if (UVerticalBoxSlot* BoxSlot = Parent->AddChildToVerticalBox(Input))
	{
		BoxSlot->SetPadding(FMargin(0.f, 3.f));
	}
	return Input;
}

void UBGSessionMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->BindDefaultButtonsInWidget(this);
	}

	// 어느 경로로 띄우든 버튼을 누를 수 있도록 입력 모드를 직접 잡는다.
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}

	Dividers = { Image_Div1, Image_Div2, Image_Div3 };
	RefreshDividerThickness();

	BindButtonEvents();

	if (IBGSessionInterface* Session = GetSession())
	{
		Session->GetSessionPhaseChangedEvent().AddUniqueDynamic(this, &UBGSessionMenuWidget::HandlePhaseChanged);
		Session->GetSessionListReadyEvent().AddUniqueDynamic(this, &UBGSessionMenuWidget::HandleSessionListReady);
	}
	else
	{
		UE_LOG(LogBGNet, Warning, TEXT("[SessionMenu] BGSessionFlow 서브시스템 없음 — 버튼이 동작하지 않는다"));
	}

	RefreshHeader();
	RefreshLobbyStatus();

	// 인원·준비 변화는 준비 컴포넌트가 알려 준다 — 폴링하지 않는다.
	LobbyChangedHandle = UBGReadyComponent::OnAnyReadyChanged.AddUObject(
		this, &UBGSessionMenuWidget::RefreshLobbyStatus);

	TryBindPhaseDelegate();
}

void UBGSessionMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 창 크기·해상도가 바뀌면 DPI 스케일도 바뀐다. 값이 달라졌을 때만 다시 맞춘다.
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (!FMath::IsNearlyEqual(Scale, LastViewportScale))
	{
		RefreshDividerThickness();
	}
}

void UBGSessionMenuWidget::RefreshDividerThickness()
{
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (Scale <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	LastViewportScale = Scale;

	//	스케일이 곱해진 뒤 정확히 SessionMenuDividerPixels 가 되도록 미리 나눠 둔다.
	const float Thickness = SessionMenuDividerPixels / Scale;
	for (UImage* Divider : Dividers)
	{
		if (!Divider)
		{
			continue;
		}
		FSlateBrush Brush = Divider->GetBrush();
		Brush.ImageSize.Y = Thickness;
		Divider->SetBrush(Brush);
	}
}

void UBGSessionMenuWidget::TryBindPhaseDelegate()
{
	ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS || BoundGameState.Get() == GS)
	{
		return;
	}

	GS->OnTurnStateChanged.AddUniqueDynamic(this, &UBGSessionMenuWidget::RefreshLobbyStatus);
	BoundGameState = GS;
}

void UBGSessionMenuWidget::RefreshLobbyStatus()
{
	//	GameState 는 클라에 늦게 온다. 갱신이 돌 때마다 다시 시도해 붙인다.
	TryBindPhaseDelegate();

	IBGSessionInterface* Session = GetSession();
	if (!Session)
	{
		return;
	}

	const bool bHost = Session->IsHost();
	const FBGLobbyStatus Status = Session->GetLobbyStatus();

	FString Reason;
	const bool bCanStart = Session->CanStartGame(Reason);

	if (LobbyText)
	{
		// 최소 인원은 서버 설정이라 클라에서는 0 으로 온다 — 그때는 인원/준비만 보여준다.
		const FString Need = (Status.RequiredPlayers > 0)
			? FString::Printf(TEXT("/%d"), Status.RequiredPlayers) : FString();

		LobbyText->SetText(FText::FromString(Status.bMatchStarted
			? TEXT("판 진행 중")
			: FString::Printf(TEXT("접속 %d%s명 · 준비 %d명"),
				Status.CurrentPlayers, *Need, Status.ReadyPlayers)));
	}

	// 페이즈가 로비를 벗어나면 로비 UI 는 물러난다. 파괴는 한 프레임 뒤로 미룬다.
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	const FGameplayTag Phase = GS ? GS->GetPhaseTag() : FGameplayTag();
	const bool bMatchBegun = Phase.IsValid() && Phase != BG::Phase::Lobby.GetTag();
	if (!bClosedOnMatchStart && bMatchBegun)
	{
		bClosedOnMatchStart = true;
		if (UWorld* World = GetWorld())
		{
			// SetTimer 는 rate 가 0 이면 등록하지 않고 해제한다 — 다음 틱은 전용 함수로 잡는다.
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					UGameInstance* GI = GetGameInstance();
					if (UBGSessionMenuController* Menu =
						GI ? GI->GetSubsystem<UBGSessionMenuController>() : nullptr)
					{
						UE_LOG(LogBGNet, Log, TEXT("[SessionMenu] 판 시작 — 로비 UI 닫음"));
						Menu->Close();
					}
				}));
		}
	}

	// 준비는 참가자만 — 호스트는 항상 준비 상태다.
	if (ReadyButton)
	{
		ReadyButton->SetVisibility(bHost ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ReadyButton->SetIsEnabled(!Status.bMatchStarted);
	}
	if (ReadyButtonText)
	{
		ReadyButtonText->SetText(FText::FromString(
			Session->IsLocalReady() ? TEXT("준비 해제") : TEXT("준비")));
	}

	// 시작은 호스트만, 전원 준비가 끝나야 눌린다.
	if (StartGameButton)
	{
		StartGameButton->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		StartGameButton->SetIsEnabled(bCanStart);
	}
}

void UBGSessionMenuWidget::NativeDestruct()
{
	UBGReadyComponent::OnAnyReadyChanged.Remove(LobbyChangedHandle);
	LobbyChangedHandle.Reset();

	if (ABGGameState* GS = BoundGameState.Get())
	{
		GS->OnTurnStateChanged.RemoveDynamic(this, &UBGSessionMenuWidget::RefreshLobbyStatus);
	}
	BoundGameState = nullptr;

	if (IBGSessionInterface* Session = GetSession())
	{
		Session->GetSessionPhaseChangedEvent().RemoveDynamic(this, &UBGSessionMenuWidget::HandlePhaseChanged);
		Session->GetSessionListReadyEvent().RemoveDynamic(this, &UBGSessionMenuWidget::HandleSessionListReady);
	}

	// 입력 모드는 되돌리지 않는다 — 카드 드래그·좌석 클릭에 마우스가 계속 필요하다.
	Super::NativeDestruct();
}

void UBGSessionMenuWidget::RefreshHeader()
{
	if (!RoomCodeText)
	{
		return;
	}

	IBGSessionInterface* Session = GetSession();
	const FString Code = Session ? Session->GetRoomCode() : FString();
	RoomCodeText->SetText(FText::FromString(
		Code.IsEmpty() ? TEXT("방 코드: -") : FString::Printf(TEXT("방 코드: %s"), *Code)));
}

void UBGSessionMenuWidget::HandleHostClicked()
{
	if (IBGSessionInterface* Session = GetSession())
	{
		const FString RoomName = RoomNameInput ? RoomNameInput->GetText().ToString() : FString();
		Session->HostCreateRoom(RoomName);
	}
}

void UBGSessionMenuWidget::HandleJoinCodeClicked()
{
	if (IBGSessionInterface* Session = GetSession())
	{
		const FString Code = JoinCodeInput ? JoinCodeInput->GetText().ToString() : FString();
		Session->JoinRoomByCode(Code);
	}
}

void UBGSessionMenuWidget::HandleRefreshClicked()
{
	if (IBGSessionInterface* Session = GetSession())
	{
		Session->RefreshSessionList(FBGSessionSearchFilter());
	}
}

void UBGSessionMenuWidget::HandleReadyClicked()
{
	if (IBGSessionInterface* Session = GetSession())
	{
		Session->SetLocalReady(!Session->IsLocalReady());
		RefreshLobbyStatus();
	}
}

void UBGSessionMenuWidget::HandleStartGameClicked()
{
	if (IBGSessionInterface* Session = GetSession())
	{
		Session->StartGame();
	}
}

void UBGSessionMenuWidget::HandleLeaveClicked()
{
	if (IBGSessionInterface* Session = GetSession())
	{
		Session->LeaveSession();
	}
}

void UBGSessionMenuWidget::JoinByIndex(int32 ResultIndex)
{
	if (IBGSessionInterface* Session = GetSession())
	{
		Session->JoinSessionByIndex(ResultIndex);
	}
}

void UBGSessionMenuWidget::HandlePhaseChanged(EBGSessionPhase Phase, const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(
			FString::Printf(TEXT("상태: %s — %s"), *PhaseToText(Phase), *Message)));
	}
	RefreshHeader();
}

void UBGSessionMenuWidget::HandleSessionListReady(const TArray<FBGSessionSearchResult>& Results)
{
	if (!ResultList)
	{
		return;
	}

	ResultList->ClearChildren();
	RowHandlers.Reset();

	if (Results.Num() == 0)
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetText(FText::FromString(TEXT("검색된 방 없음")));
		ResultList->AddChild(Empty);
		return;
	}

	for (const FBGSessionSearchResult& Item : Results)
	{
		UBGSessionRowHandler* Handler = NewObject<UBGSessionRowHandler>(this);
		Handler->Menu = this;
		Handler->ResultIndex = Item.ResultIndex;
		RowHandlers.Add(Handler);

		UButton* Row = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Row->OnClicked.AddDynamic(Handler, &UBGSessionRowHandler::HandleClicked);

		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(FString::Printf(TEXT("%s  (%d/%d)  %dms  %s"),
			*Item.RoomName, Item.CurrentPlayers, Item.MaxPlayers, Item.PingMs,
			Item.bCanJoin ? TEXT("[참여]") : TEXT("[입장 불가]"))));
		Text->SetJustification(ETextJustify::Center);
		Row->AddChild(Text);

		ResultList->AddChild(Row);
	}
}

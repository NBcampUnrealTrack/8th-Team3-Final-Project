// Fill out your copyright notice in the Description page of Project Settings.


#include "BG/Public/UI/Widget/BGChatingWidget.h"
#include "BG/Public/Chating/BGChatingSubsystem.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"

void UBGChatingWidget::NativeConstruct()
{
		Super::NativeConstruct();

		if (UGameInstance* GI = GetGameInstance())
		{
			CachedSubsystem = GI->GetSubsystem<UBGChatingSubsystem>();
		}

		if (CachedSubsystem.IsValid())
		{
			CachedSubsystem->OnMessageReceived.AddUniqueDynamic(this, &UBGChatingWidget::HandleMessageReceived);
		}

		if (ChatInputBox)
		{
			ChatInputBox->OnTextCommitted.AddUniqueDynamic(this, &UBGChatingWidget::HandleInputCommitted);

			//	여기서는 CloseChatInput 을 부르지 않는다 — 입력 모드까지 덮어써서
			//	다른 UI 가 잡아 둔 상태를 위젯이 생길 때마다 되돌려 버린다.
			ChatInputBox->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (ChatToggleButton)
		{
			ChatToggleButton->OnClicked.AddUniqueDynamic(this, &UBGChatingWidget::HandleToggleButtonClicked);
		}

		if (FilterAllButton)
		{
			FilterAllButton->OnClicked.AddUniqueDynamic(this, &UBGChatingWidget::HandleFilterAllClicked);
		}
		if (FilterPlayerButton)
		{
			FilterPlayerButton->OnClicked.AddUniqueDynamic(this, &UBGChatingWidget::HandleFilterPlayerClicked);
		}
		if (FilterSystemButton)
		{
			FilterSystemButton->OnClicked.AddUniqueDynamic(this, &UBGChatingWidget::HandleFilterSystemClicked);
		}

		bIsInputOpen = false;

		//	기존 대화 내용 로드. 필터가 걸린 상태로 그린다.
		RebuildLog();
}

void UBGChatingWidget::NativeDestruct()
{
	if (CachedSubsystem.IsValid())
	{
		CachedSubsystem->OnMessageReceived.RemoveDynamic(this, &UBGChatingWidget::HandleMessageReceived);
	}

	//	입력창을 연 채로 위젯이 사라지면 입력이 UI 에 묶인 채 남는다.
	if (bIsInputOpen)
	{
		RestoreGameInputMode();
	}

	Super::NativeDestruct();
}

void UBGChatingWidget::HandleToggleButtonClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[Chat] 버튼 OnCliked 발동"));
	ToggleChatInput();
}

void UBGChatingWidget::ToggleChatInput()
{
	UE_LOG(LogTemp, Warning, TEXT("[Chat] Toggle 호출 - 현재 열림상태: %d"), bIsInputOpen);
	
	if (bIsInputOpen)
	{
		CloseChatInput();
	}
	else
	{
		OpenChatInput();
	}
}

void UBGChatingWidget::HandleMessageReceived(const FBGChatEntry& Entry)
{
	if (PassesFilter(Entry))
	{
		AddEntryToLog(Entry);
	}
}

void UBGChatingWidget::AddEntryToLog(const FBGChatEntry& Entry)
{
	if (!ChatScrollBox)
	{
		return;
	}

	UTextBlock* Line = NewObject<UTextBlock>(this);
	Line->SetText(Entry.bSystem ? FText::FromString(Entry.Message) : FText::FromString(FString::Printf(TEXT("%s: %s"), *Entry.SenderName, *Entry.Message)));

	//	NewObject 로 만든 것이라 WBP 에서 손댈 수 없다. 여기서 정한다.
	FSlateFontInfo Font = Line->GetFont();
	Font.Size = LogFontSize;
	Line->SetFont(Font);

	Line->SetColorAndOpacity(FSlateColor(Entry.bSystem ? SystemTextColor : LogTextColor));
	Line->SetAutoWrapText(true);

	ChatScrollBox->AddChild(Line);
	ChatScrollBox->ScrollToEnd();

	TrimOldMessages();
}

void UBGChatingWidget::TrimOldMessages()
{
	if (!ChatScrollBox)
	{
		return;
	}

	while (ChatScrollBox->GetChildrenCount() > MaxVisibleMessages)
	{
		ChatScrollBox->RemoveChildAt(0);
	}
}

void UBGChatingWidget::HandleInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnCleared)
	{
		if (ChatInputBox)
		{
			ChatInputBox->SetText(FText::GetEmpty());
		}
		CloseChatInput();
		return;
	}
	
	
	//	OnTextCommitted 는 포커스를 잃어도 불린다. 엔터일 때만 보낸다.
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}

	if (!Text.IsEmpty() && CachedSubsystem.IsValid())
	{
		CachedSubsystem->SendMessage(Text.ToString());
	}

	if (ChatInputBox)
	{
		ChatInputBox->SetText(FText::GetEmpty());
	}

	CloseChatInput();
}

void UBGChatingWidget::OpenChatInput()
{
	if (bIsInputOpen)
	{
		return;
	}
	
	bIsInputOpen = true;
	
	if (ChatInputBox)
	{
		ChatInputBox->SetVisibility(ESlateVisibility::Visible);

		//	버튼 클릭 처리가 끝난 뒤에 포커스를 줘야 한다.
		//	같은 프레임에 주면 클릭이 마무리되면서 포커스가 버튼으로 되돌아간다.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick([this]()
			{
				if (bIsInputOpen && ChatInputBox)
				{
					ChatInputBox->SetKeyboardFocus();
				}
			});
		}

		if (APlayerController* PC = GetOwningPlayer())
		{
			FInputModeGameAndUI Mode;
			Mode.SetWidgetToFocus(ChatInputBox->TakeWidget());
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
			PC->SetShowMouseCursor(true);
		}
	}

	OnInputBoxOpened();
}

void UBGChatingWidget::CloseChatInput()
{
	bIsInputOpen = false;
	LastClosedFrame = GFrameCounter;

	if (ChatInputBox)
	{
		ChatInputBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	RestoreGameInputMode();

	OnInputBoxClosed();
}

void UBGChatingWidget::RestoreGameInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	//	전부 마우스로 하는 게임이라 GameOnly 가 아니다.
	//	BGSessionMenuWidget 이 잡는 것과 같은 설정으로 되돌린다.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

bool UBGChatingWidget::PassesFilter(const FBGChatEntry& Entry) const
{
	switch (ChatFilter)
	{
	case EBGChatFilter::Player: return !Entry.bSystem;
	case EBGChatFilter::System: return  Entry.bSystem;
	default:                    return true;
	}
}

void UBGChatingWidget::SetChatFilter(EBGChatFilter NewFilter)
{
	if (ChatFilter == NewFilter)
	{
		return;
	}

	ChatFilter = NewFilter;
	RebuildLog();

	OnChatFilterChanged(NewFilter);
}

void UBGChatingWidget::RebuildLog()
{
	if (!ChatScrollBox)
	{
		return;
	}

	//	보관은 History 한 곳뿐이다. 화면에 뿌린 TextBlock 은 그때그때 버리고 다시 만든다.
	ChatScrollBox->ClearChildren();

	if (!CachedSubsystem.IsValid())
	{
		return;
	}

	for (const FBGChatEntry& Entry : CachedSubsystem->GetHistory())
	{
		if (PassesFilter(Entry))
		{
			AddEntryToLog(Entry);
		}
	}
}

void UBGChatingWidget::HandleFilterAllClicked()    { SetChatFilter(EBGChatFilter::All); }
void UBGChatingWidget::HandleFilterPlayerClicked() { SetChatFilter(EBGChatFilter::Player); }
void UBGChatingWidget::HandleFilterSystemClicked() { SetChatFilter(EBGChatFilter::System); }
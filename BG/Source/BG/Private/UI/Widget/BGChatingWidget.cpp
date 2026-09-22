// Fill out your copyright notice in the Description page of Project Settings.


#include "BG/Public/UI/Widget/BGChatingWidget.h"
#include "BG/Public/Chating/BGChatingSubsystem.h"
#include "Audio/BGAudioSubsystem.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Core/BGPlayerController.h"
#include "UI/BGUIFont.h"

UBGChatingWidget::UBGChatingWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LogFont = BGUIFont::FindDefault();
}

void UBGChatingWidget::NativeConstruct()
{
		Super::NativeConstruct();
		if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
		{
			Audio->BindDefaultButtonsInWidget(this);
		}

		//	HUD 안에 배치되면 PlayerController 가 CreateWidget 으로 이 인스턴스를 잡을 기회가
		//	없다. Enter 로 입력창 열기(HandleChatKeyPressed)가 이 참조를 쓰므로 스스로 등록한다.
		if (ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwningPlayer()))
		{
				PC->ChatWidgetInstance = this;
		}

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

			//	입력창은 항상 떠 있다 — 클릭하면 바로 타이핑할 수 있다.
			//	여기서 입력 모드를 건드리지 않는다. 위젯이 생길 때마다
			//	다른 UI 가 잡아 둔 상태를 되돌려 버린다.
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

		RefreshFilterButtons();

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

	//	긴 단어는 줄 맨 앞에서만 쪼개진다(TextLayout.cpp:625) — 이름 뒤에 붙으면 통째로 다음 줄로 밀린다.
	//	공백 없이 길게 이어지는 자리에만 폭 0 문자를 끼워 줄바꿈 기회를 준다. 자리가 남으면 끊지 않는다.
	FString Body = Entry.Message;
	for (int32 i = 0, Run = 0; i < Body.Len(); ++i)
	{
		if (FChar::IsWhitespace(Body[i])) { Run = 0; }
		else if (++Run > 12) { Body.InsertAt(i++, TCHAR(0x200B)); Run = 0; }
	}
	Line->SetText(Entry.bSystem ? FText::FromString(Body) : FText::FromString(FString::Printf(TEXT("%s: %s"), *Entry.SenderName, *Body)));

	//	NewObject 로 만든 것이라 WBP 에서 손댈 수 없다. 여기서 정한다.
	FSlateFontInfo Font = Line->GetFont();
	Font.Size = LogFontSize;
	BGUIFont::Apply(Font, LogFont);
	Line->SetFont(Font);

	Line->SetColorAndOpacity(FSlateColor(Entry.bSystem ? SystemTextColor : LogTextColor));
	Line->SetAutoWrapText(true);
	//	기본 정책은 띄어쓰기에서만 끊는다 — 공백 없는 긴 채팅이 통째로 삐져나간다.
	Line->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);

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
		//	포커스를 잃었으면 더는 열린 상태가 아니다 — 안 내리면 Enter 가 영영 막힌다.
		bIsInputOpen = false;
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
		//	키 입력 처리가 끝난 뒤에 포커스를 줘야 한다.
		//	같은 프레임에 주면 그 입력이 마무리되면서 포커스가 되돌아간다.
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

	//	입력창은 그대로 두고 포커스만 게임으로 돌린다.
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

void UBGChatingWidget::RefreshFilterButtons()
{
	const TTuple<UButton*, EBGChatFilter> Tabs[] = {
		{ FilterAllButton, EBGChatFilter::All },
		{ FilterPlayerButton, EBGChatFilter::Player },
		{ FilterSystemButton, EBGChatFilter::System } };

	for (const TTuple<UButton*, EBGChatFilter>& Tab : Tabs)
	{
		UTexture2D* Texture = (ChatFilter == Tab.Get<1>()) ? FilterTabActive : FilterTabInactive;
		if (!Tab.Get<0>() || !Texture)
		{
			continue;
		}

		//	그림만 갈아끼운다 — 틴트로 준 호버/눌림 차이는 WBP 설정 그대로 남는다.
		FButtonStyle Style = Tab.Get<0>()->GetStyle();
		Style.Normal.SetResourceObject(Texture);
		Style.Hovered.SetResourceObject(Texture);
		Style.Pressed.SetResourceObject(Texture);
		Tab.Get<0>()->SetStyle(Style);
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
	RefreshFilterButtons();

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

#include "UI/Widget/BGRewardSelectWidget.h"

#include "Audio/BGAudioSubsystem.h"
#include "Components/Button.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "Card/BGCardType.h"
#include "UI/View/BGCardViewLibrary.h"
#include "UI/Widget/BGCardWidget.h"

void UBGRewardSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->BindDefaultButtonsInWidget(this);
	}

	if (CardButton0)
	{
		CardButton0->OnClicked.AddDynamic(this, &UBGRewardSelectWidget::HandleCard0Clicked);
	}
	if (CardButton1)
	{
		CardButton1->OnClicked.AddDynamic(this, &UBGRewardSelectWidget::HandleCard1Clicked);
	}
	if (CardButton2)
	{
		CardButton2->OnClicked.AddDynamic(this, &UBGRewardSelectWidget::HandleCard2Clicked);
	}

	//	카드 위젯 자체는 드래그 전용이다. 드래그를 잠가서 클릭이 감싸고 있는 Button 까지
	//	그대로 전달되게 한다(UBGCardWidget::NativeOnMouseButtonDown 참고).
	if (CardWidget0)
	{
		CardWidget0->SetDragLocked(true);
	}
	if (CardWidget1)
	{
		CardWidget1->SetDragLocked(true);
	}
	if (CardWidget2)
	{
		CardWidget2->SetDragLocked(true);
	}

	//	바인딩 + 지금 이미 후보가 있으면 바로 채워 보여준다.
	HandleTurnStateChanged();
}

void UBGRewardSelectWidget::NativeDestruct()
{
	if (ABGGameState* GS = BoundGameState.Get())
	{
		GS->OnTurnStateChanged.RemoveDynamic(this, &UBGRewardSelectWidget::HandleTurnStateChanged);
	}

	Super::NativeDestruct();
}

void UBGRewardSelectWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UBGCardWidget* Widgets[3] = { CardWidget0, CardWidget1, CardWidget2 };
	for (UBGCardWidget* CardWidget : Widgets)
	{
		if (IsValid(CardWidget))
		{
			CardWidget->TickMotion(InDeltaTime);
		}
	}
}

void UBGRewardSelectWidget::HandleCard0Clicked()
{
	SelectSlot(0);
}

void UBGRewardSelectWidget::HandleCard1Clicked()
{
	SelectSlot(1);
}

void UBGRewardSelectWidget::HandleCard2Clicked()
{
	SelectSlot(2);
}

void UBGRewardSelectWidget::SelectSlot(int32 SlotIndex)
{
	if (!SlotCardIds.IsValidIndex(SlotIndex))
	{
		return;
	}

	if (ABGPlayerController* PC = GetOwningPlayer<ABGPlayerController>())
	{
		PC->RequestSelectPvEReward(SlotCardIds[SlotIndex]);
	}
}

void UBGRewardSelectWidget::RefreshCandidates()
{
	const ABGGameState* GS = BoundGameState.Get();
	const TArray<FName> Candidates = GS ? GS->GetPendingRewardCandidates() : TArray<FName>();

	SlotCardIds = Candidates;

	UButton* Buttons[3] = { CardButton0, CardButton1, CardButton2 };
	UBGCardWidget* Widgets[3] = { CardWidget0, CardWidget1, CardWidget2 };

	for (int32 i = 0; i < 3; ++i)
	{
		if (!ensureMsgf(Buttons[i] && Widgets[i],
			TEXT("[BG] 보상 선택 위젯 슬롯 %d 의 BindWidget 이 비어있습니다. WBP 를 확인하세요."), i))
		{
			continue;
		}

		if (Candidates.IsValidIndex(i))
		{
			Buttons[i]->SetVisibility(ESlateVisibility::Visible);

			//	보상 후보는 아직 손패에 들어간 실제 덱 카드가 아니다. InstanceId 는 슬롯을
			//	구분하기 위한 값일 뿐이라 음수로 둔다(실제 덱 InstanceId는 1부터 시작한다).
			FBGCardInstance RewardCard;
			RewardCard.InstanceId = -(i + 1);
			RewardCard.CardId = Candidates[i];

			Widgets[i]->SetCardView(UBGCardViewLibrary::MakeCardView(this, RewardCard));
		}
		else
		{
			Buttons[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UBGRewardSelectWidget::TryBindTurnStateDelegate()
{
	ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS || BoundGameState.Get() == GS)
	{
		return;
	}

	GS->OnTurnStateChanged.AddUniqueDynamic(this, &UBGRewardSelectWidget::HandleTurnStateChanged);
	BoundGameState = GS;
}

void UBGRewardSelectWidget::HandleTurnStateChanged()
{
	TryBindTurnStateDelegate();

	const ABGGameState* GS = BoundGameState.Get();
	if (!GS)
	{
		return;
	}

	if (GS->IsMatchRunning() || GS->GetPendingRewardCandidates().Num() == 0)
	{
		//	판이 시작됐거나 이미 다 골랐다. 이 화면은 더 필요 없다.
		RemoveFromParent();
		return;
	}

	RefreshCandidates();
}

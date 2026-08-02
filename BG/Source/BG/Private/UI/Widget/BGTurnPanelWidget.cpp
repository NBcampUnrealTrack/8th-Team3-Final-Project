#include "UI/Widget/BGTurnPanelWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UBGTurnPanelWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshVisual();
}

void UBGTurnPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_EndTurn)
	{
		//	중복 구독 방지 — NativeConstruct 는 위젯이 다시 뷰포트에 붙을 때마다 불릴 수 있다.
		Button_EndTurn->OnClicked.RemoveDynamic(this, &UBGTurnPanelWidget::HandleEndTurnClicked);
		Button_EndTurn->OnClicked.AddDynamic(this, &UBGTurnPanelWidget::HandleEndTurnClicked);
	}

	RefreshVisual();
}

void UBGTurnPanelWidget::SetTurnView(const FBGTurnView& InTurnView)
{
	TurnView = InTurnView;
	RefreshVisual();
}

void UBGTurnPanelWidget::HandleEndTurnClicked()
{
	//	버튼이 눌렸다는 사실만 알린다. 실제 검증은 서버가 한다.
	OnEndTurnRequested.Broadcast();
}

void UBGTurnPanelWidget::RefreshVisual()
{
	if (Text_Status)
	{
		Text_Status->SetText(TurnView.StatusText);
	}

	if (Text_HandLimit)
	{
		//	한도가 없으면(설정 문제로 Health 를 못 읽은 경우) 숫자 대신 '-' 로 표시한다.
		Text_HandLimit->SetText(TurnView.HandLimit == INDEX_NONE
			? FText::FromString(FString::Printf(TEXT("손패 %d"), TurnView.HandCount))
			: FText::FromString(FString::Printf(TEXT("손패 %d / 한도 %d"), TurnView.HandCount, TurnView.HandLimit)));
	}

	if (Text_Deck)
	{
		Text_Deck->SetText(FText::FromString(FString::Printf(TEXT("덱 %d"), TurnView.DeckCount)));
	}

	if (Text_CardUse)
	{
		//	GDD §10: 턴 UI 에 "사용한 카드 수 / 현재 카드 사용 한도" 를 표시한다.
		Text_CardUse->SetText(FText::FromString(
			FString::Printf(TEXT("카드 %d / %d"), TurnView.CardsUsed, TurnView.CardUseLimit)));
	}

	if (Button_EndTurn)
	{
		//	버리기 페이즈에서는 턴 종료를 누를 수 없다 — 카드를 버려야 넘어간다.
		//	그래서 활성 조건은 bIsMyTurn(= Play 페이즈) 뿐이다.
		Button_EndTurn->SetIsEnabled(TurnView.bIsMyTurn);
	}

	if (bHideWhenNotMyTurn)
	{
		SetVisibility((TurnView.bIsMyTurn || TurnView.bIsMyDiscard)
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

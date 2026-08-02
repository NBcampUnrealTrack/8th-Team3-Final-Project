// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widget/BGHandWidget.h"
#include "UI/Widget/BGCardWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogBGUI, Log, All);

void UBGHandWidget::SetHand(const TArray<FBGCardView>& InCards)
{
	ClearHand();

	if (!Box_Cards)
	{
		UE_LOG(LogBGUI, Error, TEXT("%s: Box_Cards 바인딩이 없습니다. WBP 에 'Box_Cards' 패널을 추가하세요."), *GetName());
		return;
	}

	if (!CardWidgetClass)
	{
		UE_LOG(LogBGUI, Error, TEXT("%s: CardWidgetClass 가 비어 있습니다. 카드 WBP 를 지정하세요."), *GetName());
		return;
	}

	CardWidgets.Reserve(InCards.Num());
	for (const FBGCardView& Card : InCards)
	{
		UBGCardWidget* CardWidget = CreateWidget<UBGCardWidget>(this, CardWidgetClass);
		if (!CardWidget)
		{
			continue;
		}

		// SetOwningHand 는 SetCardView 보다 먼저. 드래그 시작 시 핸드를 알아야 한다.
		CardWidget->SetOwningHand(this);
		CardWidget->SetCardView(Card);

		//	잠긴 동안 손패가 갱신될 수 있다(서버 복제와 대상 선택은 동시에 일어난다).
		//	새 위젯에도 잠금을 그대로 물려주지 않으면 갱신 한 번에 잠금이 풀린다.
		CardWidget->SetDragLocked(bInteractionLocked);

		// 'Slot' 은 UWidget 의 멤버명이므로 섀도잉을 피한다 (C4458 은 에러로 승격됨).
		UPanelSlot* AddedSlot = Box_Cards->AddChild(CardWidget);
		if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(AddedSlot))
		{
			HBoxSlot->SetPadding(CardPadding);
			HBoxSlot->SetVerticalAlignment(VAlign_Bottom);
		}

		CardWidgets.Add(CardWidget);
	}
}

void UBGHandWidget::ClearHand()
{
	if (Box_Cards)
	{
		Box_Cards->ClearChildren();
	}
	CardWidgets.Reset();
}

bool UBGHandWidget::RemoveCardByInstanceId(int32 InstanceId)
{
	for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
	{
		UBGCardWidget* CardWidget = CardWidgets[Index];
		if (CardWidget && CardWidget->GetInstanceId() == InstanceId)
		{
			CardWidget->RemoveFromParent();
			CardWidgets.RemoveAt(Index);
			return true;
		}
	}
	return false;
}

UBGCardWidget* UBGHandWidget::FindCardWidget(int32 InstanceId) const
{
	for (UBGCardWidget* CardWidget : CardWidgets)
	{
		if (CardWidget && CardWidget->GetInstanceId() == InstanceId)
		{
			return CardWidget;
		}
	}
	return nullptr;
}

void UBGHandWidget::SetInteractionLocked(bool bInLocked)
{
	bInteractionLocked = bInLocked;

	for (UBGCardWidget* CardWidget : CardWidgets)
	{
		if (CardWidget)
		{
			CardWidget->SetDragLocked(bInLocked);
		}
	}
}

void UBGHandWidget::NotifyCardPlayRequested(const FBGCardView& Card)
{
	if (!Card.IsValid())
	{
		return;
	}

	UE_LOG(LogBGUI, Verbose, TEXT("카드 사용 요청: InstanceId=%d CardId=%s NeedsTarget=%s"),
		Card.InstanceId, *Card.CardId.ToString(), Card.bNeedsTarget ? TEXT("true") : TEXT("false"));

	// 여기서 카드를 제거하지 않는다. 손패에서 빠지는 시점은 서버가 결정한다.
	OnCardPlayRequested.Broadcast(Card);
}

void UBGHandWidget::FillWithDebugCards(int32 Count)
{
	static const TCHAR* DebugNames[] = { TEXT("BANG!"), TEXT("빗나감!"), TEXT("맥주"), TEXT("역마차"), TEXT("결투"), TEXT("개틀링"), TEXT("술집") };
	static const TCHAR* DebugDescs[] = {
		TEXT("사거리 내의 플레이어 1명에게 1 피해."),
		TEXT("자신을 향한 BANG! 을 무효화한다."),
		TEXT("생명력 1 회복. 2인만 남으면 효과가 없다."),
		TEXT("카드 2장을 뽑는다."),
		TEXT("대상과 번갈아 BANG! 을 낸다. 먼저 못 내면 1 피해."),
		TEXT("자신을 제외한 모든 플레이어에게 BANG!"),
		TEXT("자신을 제외한 모든 플레이어가 생명력 1 회복."),
	};
	constexpr int32 NumDebugCards = UE_ARRAY_COUNT(DebugNames);

	TArray<FBGCardView> Cards;
	Cards.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Pick = Index % NumDebugCards;

		FBGCardView Card;
		Card.InstanceId   = Index + 1;            // 0 이 아닌 유효 ID
		Card.CardId       = FName(DebugNames[Pick]);
		Card.DisplayName  = FText::FromString(DebugNames[Pick]);
		Card.Description  = FText::FromString(DebugDescs[Pick]);
		Card.bNeedsTarget = (Pick == 0 || Pick == 4);
		Cards.Add(Card);
	}

	SetHand(Cards);
}

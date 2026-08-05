// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widget/BGPlayZoneWidget.h"
#include "UI/Widget/BGCardDragDropOperation.h"
#include "UI/Widget/BGHandWidget.h"
#include "Components/Border.h"

void UBGPlayZoneWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	if (const UBGCardDragDropOperation* CardOp = Cast<UBGCardDragDropOperation>(InOperation))
	{
		SetHighlight(CardOp->CardView.bPlayable ? HoverColor : RejectColor);
	}
}

void UBGPlayZoneWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	SetHighlight(IdleColor);
}

bool UBGPlayZoneWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetHighlight(IdleColor);

	UBGCardDragDropOperation* CardOp = Cast<UBGCardDragDropOperation>(InOperation);
	if (!CardOp || !CardOp->CardView.IsValid())
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	// 클라이언트 힌트로 명백히 불가한 카드는 요청조차 보내지 않는다.
	// 최종 판정은 언제나 서버가 한다.
	if (!CardOp->CardView.bPlayable)
	{
		// 조용히 무시하면 "왜 카드가 안 나가지" 로 시간을 버린다.
		// bPlayable 은 MakeHandViews 가 턴/페이즈를 보고 정한다 — 여기가 찍히는데
		// 자기 차례가 맞다면 GameState 의 턴 상태가 클라까지 오지 않은 것이다.
		UE_LOG(LogTemp, Warning,
			TEXT("[BG] 플레이 존: 카드 %s(#%d) 가 bPlayable=false 라 드롭을 거부했습니다 "
			     "(자기 차례가 아니거나, 턴 상태가 클라에 복제되지 않음)."),
			*CardOp->CardView.CardId.ToString(), CardOp->CardView.InstanceId);
		return true;
	}

	// 정식 경로는 원본 핸드 위젯이다. 플레이 존 델리게이트는 보조 훅.
	//
	// 좌표까지 넘기는 이유: 드래그 비주얼은 드롭 순간 사라진다. 손패가 "어디에 놓았는지"
	// 를 모르면 원본 카드가 손패 자리에서 다시 출발해, 놓은 카드가 사라졌다가
	// 중앙에 다시 나타나는 것처럼 보인다.
	if (UBGHandWidget* Hand = CardOp->SourceHandWidget.Get())
	{
		Hand->NotifyCardDroppedOnPlayZone(
			CardOp->CardView, InGeometry, InDragDropEvent.GetScreenSpacePosition());
	}
	OnCardDropped.Broadcast(CardOp->CardView);

	return true;
}

void UBGPlayZoneWidget::SetHighlight(const FLinearColor& Color)
{
	if (Border_Highlight)
	{
		Border_Highlight->SetBrushColor(Color);
	}
}

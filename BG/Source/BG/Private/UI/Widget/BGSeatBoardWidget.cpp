#include "UI/Widget/BGSeatBoardWidget.h"

#include "UI/Widget/BGSeatWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogBGSeatUI, Log, All);

void UBGSeatBoardWidget::SetSeats(const TArray<FBGSeatView>& InSeats)
{
	if (!Panel_Seats)
	{
		UE_LOG(LogBGSeatUI, Error, TEXT("%s: Panel_Seats 바인딩이 없습니다."), *GetName());
		return;
	}

	if (!SeatWidgetClass)
	{
		UE_LOG(LogBGSeatUI, Error, TEXT("%s: SeatWidgetClass 가 비어 있습니다. 좌석 WBP 를 지정하세요."), *GetName());
		return;
	}

	//	이번에 들어온 좌석 번호를 모아 둔다. 여기 없는 기존 위젯은 아래에서 제거한다.
	TSet<int32> IncomingSeats;
	IncomingSeats.Reserve(InSeats.Num());

	bool bLayoutChanged = false;

	for (const FBGSeatView& Seat : InSeats)
	{
		if (!Seat.IsValid())
		{
			continue;
		}
		IncomingSeats.Add(Seat.SeatIndex);

		//	같은 좌석 번호면 위젯을 재사용한다 — 값만 바꾸므로 호버/선택 상태가 살아남는다.
		//	주기적으로 SetSeats 를 불러도 조작이 끊기지 않는 이유가 여기에 있다.
		if (TObjectPtr<UBGSeatWidget>* Existing = SeatWidgets.Find(Seat.SeatIndex))
		{
			if (*Existing)
			{
				(*Existing)->SetSeatView(Seat);
				continue;
			}
		}

		UBGSeatWidget* SeatWidget = CreateWidget<UBGSeatWidget>(this, SeatWidgetClass);
		if (!SeatWidget)
		{
			continue;
		}

		SeatWidget->SetOwningBoard(this);
		SeatWidget->SetSeatView(Seat);

		//	'Slot' 은 UWidget 의 멤버명이라 섀도잉을 피한다.
		UPanelSlot* AddedSlot = Panel_Seats->AddChild(SeatWidget);
		if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(AddedSlot))
		{
			HBoxSlot->SetPadding(SeatPadding);
		}

		SeatWidgets.Add(Seat.SeatIndex, SeatWidget);
		bLayoutChanged = true;
	}

	//	사라진 좌석 정리(플레이어 퇴장 등). 사망은 좌석이 남으므로 여기 걸리지 않는다.
	for (auto It = SeatWidgets.CreateIterator(); It; ++It)
	{
		if (IncomingSeats.Contains(It.Key()))
		{
			continue;
		}
		if (UBGSeatWidget* Stale = It.Value())
		{
			Stale->RemoveFromParent();
		}
		It.RemoveCurrent();
		bLayoutChanged = true;
	}

	if (bLayoutChanged && bSelectingTarget)
	{
		//	좌석이 새로 생겼다면 선택 가능 표시를 다시 입혀야 한다.
		ApplySelectionState();
	}
}

void UBGSeatBoardWidget::ClearSeats()
{
	if (Panel_Seats)
	{
		Panel_Seats->ClearChildren();
	}
	SeatWidgets.Reset();
}

UBGSeatWidget* UBGSeatBoardWidget::FindSeatWidget(int32 SeatIndex) const
{
	const TObjectPtr<UBGSeatWidget>* Found = SeatWidgets.Find(SeatIndex);
	return Found ? *Found : nullptr;
}

// ======================================================================================
//  대상 선택
// ======================================================================================

void UBGSeatBoardWidget::BeginTargetSelection(const FBGTargetRequest& Request)
{
	//	이미 다른 선택이 진행 중이면 그쪽 호출자에게 취소를 알리고 넘겨받는다.
	//	알리지 않으면 이전 요청을 기다리던 코드가 영원히 응답을 못 받는다.
	if (bSelectingTarget)
	{
		const int32 PreviousContext = ActiveRequest.ContextId;
		UE_LOG(LogBGSeatUI, Warning, TEXT("이전 대상 선택(Context %d)이 새 요청으로 대체됩니다."), PreviousContext);
		OnTargetSelectionCancelled.Broadcast(PreviousContext);
	}

	ActiveRequest     = Request;
	bSelectingTarget  = true;

	if (Request.SelectableSeats.IsEmpty())
	{
		//	고를 수 있는 좌석이 없다 — 사거리 밖이거나 생존자가 없다. 즉시 취소로 끝낸다.
		UE_LOG(LogBGSeatUI, Warning, TEXT("선택 가능한 좌석이 없습니다(Context %d) — 요청을 취소합니다."), Request.ContextId);
		CancelTargetSelection();
		return;
	}

	ApplySelectionState();
	UpdatePromptVisibility();
}

void UBGSeatBoardWidget::CancelTargetSelection()
{
	if (!bSelectingTarget)
	{
		return;
	}

	const int32 ContextId = ActiveRequest.ContextId;

	bSelectingTarget = false;
	ActiveRequest = FBGTargetRequest();

	ApplySelectionState();
	UpdatePromptVisibility();

	OnTargetSelectionCancelled.Broadcast(ContextId);
}

void UBGSeatBoardWidget::NotifySeatClicked(int32 SeatIndex)
{
	if (!bSelectingTarget)
	{
		return;   // 평소 클릭은 무시한다.
	}

	//	위젯 쪽에서도 막고 있지만, 여기서 한 번 더 확인한다 —
	//	SetSeats 로 좌석 상태가 바뀌는 사이에 클릭이 들어올 수 있다.
	if (!ActiveRequest.SelectableSeats.Contains(SeatIndex))
	{
		return;
	}

	const int32 ContextId = ActiveRequest.ContextId;

	//	브로드캐스트 전에 모드를 먼저 닫는다. 수신 측이 곧바로 다음 선택을 시작할 수 있기 때문.
	bSelectingTarget = false;
	ActiveRequest = FBGTargetRequest();

	ApplySelectionState();
	UpdatePromptVisibility();

	OnSeatSelected.Broadcast(SeatIndex, ContextId);
}

void UBGSeatBoardWidget::ApplySelectionState()
{
	for (const TPair<int32, TObjectPtr<UBGSeatWidget>>& Pair : SeatWidgets)
	{
		UBGSeatWidget* SeatWidget = Pair.Value;
		if (!SeatWidget)
		{
			continue;
		}

		//	죽은 좌석은 어떤 경우에도 대상이 될 수 없다.
		const bool bSelectable = bSelectingTarget
			&& SeatWidget->GetSeatView().bIsAlive
			&& ActiveRequest.SelectableSeats.Contains(Pair.Key);

		SeatWidget->SetSelectable(bSelectable);
	}
}

void UBGSeatBoardWidget::UpdatePromptVisibility()
{
	if (Text_Prompt)
	{
		Text_Prompt->SetText(bSelectingTarget ? ActiveRequest.Prompt : FText::GetEmpty());
	}

	if (Panel_Prompt)
	{
		Panel_Prompt->SetVisibility(bSelectingTarget
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

// ======================================================================================
//  디버그
// ======================================================================================

void UBGSeatBoardWidget::FillWithDebugSeats(int32 Count)
{
	Count = FMath::Clamp(Count, 1, 7);

	TArray<FBGSeatView> Seats;
	Seats.Reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		FBGSeatView& Seat = Seats.AddDefaulted_GetRef();
		Seat.SeatIndex      = i;
		Seat.PlayerName     = FText::FromString(FString::Printf(TEXT("플레이어 %d"), i));
		Seat.MaxHealth      = (i == 0) ? 5 : 4;
		Seat.Health         = FMath::Max(1, Seat.MaxHealth - i);
		Seat.HandCount      = 5 - (i % 3);
		Seat.bIsAlive       = true;
		Seat.bIsLocalPlayer = (i == 0);
		Seat.bIsCurrentTurn = (i == 0);
		Seat.RoleName       = (i == 0) ? FText::FromString(TEXT("보안관")) : FText::GetEmpty();
		Seat.Distance       = (i == 0) ? INDEX_NONE : FMath::Min(i, Count - i);
	}

	SetSeats(Seats);
}

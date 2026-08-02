#include "UI/Widget/BGSeatWidget.h"

#include "UI/Widget/BGSeatBoardWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

void UBGSeatWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshVisual();
}

void UBGSeatWidget::SetOwningBoard(UBGSeatBoardWidget* InBoard)
{
	OwningBoard = InBoard;
}

void UBGSeatWidget::SetSeatView(const FBGSeatView& InSeatView)
{
	SeatView = InSeatView;
	RefreshVisual();
}

void UBGSeatWidget::SetSelectable(bool bInSelectable)
{
	if (bSelectable == bInSelectable)
	{
		return;
	}
	bSelectable = bInSelectable;
	if (!bSelectable)
	{
		bHovered = false;
	}
	RefreshVisual();
}

void UBGSeatWidget::RefreshVisual()
{
	if (Text_PlayerName)
	{
		Text_PlayerName->SetText(SeatView.PlayerName);
	}

	if (Text_Health)
	{
		Text_Health->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d"), SeatView.Health, SeatView.MaxHealth)));
	}

	if (Text_HandCount)
	{
		//	남의 손패는 장수만 안다. 내용은 여기 오지 않는다.
		Text_HandCount->SetText(FText::FromString(
			FString::Printf(TEXT("손패 %d"), SeatView.HandCount)));
	}

	if (Text_Distance)
	{
		//	자기 자신이거나 계산 불가면 "-". 사거리 판정 디버깅의 주력 표시다.
		Text_Distance->SetText(SeatView.Distance == INDEX_NONE
			? FText::FromString(TEXT("-"))
			: FText::FromString(FString::Printf(TEXT("거리 %d"), SeatView.Distance)));
	}

	if (Text_Role)
	{
		//	비공개 역할은 빈 텍스트다 — 여기에 값이 차 있으면 공개된 좌석이라는 뜻.
		Text_Role->SetText(SeatView.RoleName);
	}

	if (Text_Equipment)
	{
		TArray<FString> Names;
		Names.Reserve(SeatView.EquipmentNames.Num());
		for (const FText& Name : SeatView.EquipmentNames)
		{
			Names.Add(Name.ToString());
		}
		Text_Equipment->SetText(FText::FromString(FString::Join(Names, TEXT(", "))));
	}

	if (!Border_Root)
	{
		return;
	}

	//	우선순위: 사망 > 선택 모드 > 현재 턴 > 본인 > 기본.
	//	사망을 맨 위에 두는 이유 — 죽은 좌석은 어떤 경우에도 대상이 될 수 없다.
	FLinearColor Tint;
	if (!SeatView.bIsAlive)
	{
		Tint = DeadColor;
	}
	else if (bSelectable)
	{
		Tint = bHovered ? SelectableHoverColor : SelectableColor;
	}
	else if (OwningBoard.IsValid() && OwningBoard->IsSelectingTarget())
	{
		//	선택 모드인데 이 좌석은 못 고른다 → 사거리 밖임을 눈으로 알 수 있게 어둡게.
		Tint = NotSelectableColor;
	}
	else if (SeatView.bIsCurrentTurn)
	{
		Tint = CurrentTurnColor;
	}
	else if (SeatView.bIsLocalPlayer)
	{
		Tint = LocalPlayerColor;
	}
	else
	{
		Tint = NormalColor;
	}

	Border_Root->SetBrushColor(Tint);
}

FReply UBGSeatWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	//	평소에는 클릭을 먹지 않는다. 대상 선택 모드에서 고를 수 있는 좌석일 때만 반응한다.
	if (!bSelectable || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (UBGSeatBoardWidget* Board = OwningBoard.Get())
	{
		Board->NotifySeatClicked(SeatView.SeatIndex);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void UBGSeatWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (bSelectable)
	{
		bHovered = true;
		RefreshVisual();
	}
}

void UBGSeatWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (bHovered)
	{
		bHovered = false;
		RefreshVisual();
	}
}

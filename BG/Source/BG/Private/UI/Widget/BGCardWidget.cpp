// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widget/BGCardWidget.h"
#include "UI/Widget/BGCardDragDropOperation.h"
#include "UI/Widget/BGHandWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

void UBGCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshVisual();
}

void UBGCardWidget::SetOwningHand(UBGHandWidget* InHand)
{
	OwningHand = InHand;
}

void UBGCardWidget::SetCardView(const FBGCardView& InCardView)
{
	CardView = InCardView;
	RefreshVisual();
}

void UBGCardWidget::SetBeingDragged(bool bInBeingDragged)
{
	if (bBeingDragged == bInBeingDragged)
	{
		return;
	}
	bBeingDragged = bInBeingDragged;
	RefreshVisual();
}

void UBGCardWidget::SetDragLocked(bool bInLocked)
{
	if (bDragLocked == bInLocked)
	{
		return;
	}
	bDragLocked = bInLocked;
	RefreshVisual();
}

void UBGCardWidget::RefreshChanceText()
{
	if (!Text_Chance && !Text_ChanceDetail)
	{
		return;   // WBP 가 확률 칸을 두지 않았다. 선택 사항이므로 조용히 넘어간다.
	}

	//	판정하지 않는 카드(장비)에 확률을 띄우면 "장비도 실패할 수 있다" 는 오해를 준다.
	//	Collapsed 로 자리까지 회수한다 — 장비 카드에 빈 줄이 남지 않도록.
	const ESlateVisibility ChanceVisibility =
		CardView.bHasChances ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	//	보정이 걸렸으면 보정 후 값을 보여준다. GDD §8 이 요구하는 것은 "최종 확률" 이다.
	const FBGCardChanceView& Shown = CardView.FinalChance;
	const FLinearColor Tint = CardView.bChanceModified ? ModifiedChanceColor : NormalChanceColor;

	if (Text_Chance)
	{
		Text_Chance->SetVisibility(ChanceVisibility);
		if (CardView.bHasChances)
		{
			Text_Chance->SetText(FText::Format(
				NSLOCTEXT("BG", "CardChanceShort", "성공 {0}%"),
				FText::AsNumber(FMath::RoundToInt(Shown.GetSuccessOrBetter()))));
			Text_Chance->SetColorAndOpacity(FSlateColor(Tint));
		}
	}

	if (Text_ChanceDetail)
	{
		Text_ChanceDetail->SetVisibility(ChanceVisibility);
		if (CardView.bHasChances)
		{
			//	반올림 때문에 합이 100 이 안 될 수 있다. 각 등급을 정확히 보여주는 쪽이
			//	"합 100 맞추기" 보다 중요하다 — 플레이어가 비교하는 것은 등급별 값이다.
			Text_ChanceDetail->SetText(FText::Format(
				NSLOCTEXT("BG", "CardChanceDetail", "대실패 {0} · 실패 {1} · 성공 {2} · 대성공 {3}"),
				FText::AsNumber(FMath::RoundToInt(Shown.CriticalFailure)),
				FText::AsNumber(FMath::RoundToInt(Shown.Failure)),
				FText::AsNumber(FMath::RoundToInt(Shown.Success)),
				FText::AsNumber(FMath::RoundToInt(Shown.CriticalSuccess))));
			Text_ChanceDetail->SetColorAndOpacity(FSlateColor(Tint));
		}
	}
}

void UBGCardWidget::RefreshVisual()
{
	if (Text_CardName)
	{
		Text_CardName->SetText(CardView.DisplayName);
	}
	if (Text_CardDescription)
	{
		Text_CardDescription->SetText(CardView.Description);
	}

	RefreshChanceText();
	if (Border_Root)
	{
		//	잠금이 가장 우선 — 잠긴 동안에는 사용 가능 여부와 무관하게 손댈 수 없다.
		const FLinearColor Tint =
			bDragLocked          ? LockedTint :
			bBeingDragged        ? DraggedTint :
			!CardView.bPlayable  ? UnplayableTint :
			                       NormalTint;
		Border_Root->SetBrushColor(Tint);
	}
}

FReply UBGCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 드래그 비주얼 복제본은 입력을 받지 않는다.
	// 잠금 중(대상 선택 대기 등)에도 드래그를 시작시키지 않는다.
	if (bIsDragVisual || bDragLocked)
	{
		return FReply::Unhandled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// 즉시 드래그하지 않고 임계 거리를 넘어야 드래그로 승격된다.
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UBGCardWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (bIsDragVisual || !CardView.IsValid())
	{
		return;
	}

	UBGCardDragDropOperation* Operation = Cast<UBGCardDragDropOperation>(
		UWidgetBlueprintLibrary::CreateDragDropOperation(UBGCardDragDropOperation::StaticClass()));
	if (!Operation)
	{
		return;
	}

	Operation->CardView         = CardView;
	Operation->SourceCardWidget = this;
	Operation->SourceHandWidget = OwningHand;
	Operation->Pivot            = EDragPivot::MouseDown;

	// 커서를 따라다닐 복제 위젯. DragVisualClass 가 없으면 자기 클래스를 쓴다.
	UClass* VisualClass = DragVisualClass ? DragVisualClass.Get() : GetClass();
	if (UBGCardWidget* Visual = CreateWidget<UBGCardWidget>(this, VisualClass))
	{
		Visual->bIsDragVisual = true;
		Visual->SetCardView(CardView);
		Operation->DefaultDragVisual = Visual;
	}

	SetBeingDragged(true);
	OutOperation = Operation;
}

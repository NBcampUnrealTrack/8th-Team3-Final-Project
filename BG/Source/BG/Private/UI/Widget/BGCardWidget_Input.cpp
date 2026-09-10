// UBGCardWidget — 마우스 입력 (호버 판정 · 드래그 시작).
//
// 슬레이트가 주는 이벤트를 카드의 상태(bHovered / HoverLocalNormalized)로 옮기는 것까지가
// 이 파일의 일이다. 그 값으로 무엇을 그릴지는 모션 쪽(TickSecondaryMotion)이 정한다 —
// 입력과 연출이 한 함수에 있으면 "커서를 안 움직여도 풀려야 하는" 경우를 놓친다.

#include "UI/Widget/BGCardWidget.h"
#include "UI/Widget/BGCardDragDropOperation.h"
#include "UI/Widget/BGHandWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"

// ── 호버 입력 ────────────────────────────────────────────────────────────────

void UBGCardWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (bIsDragVisual)
	{
		return;   // 커서를 따라다니는 복제본은 반응하지 않는다
	}

	bHovered = true;
}

void UBGCardWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	bHovered = false;

	//	커서 위치는 남겨 둔다. 0 으로 리셋하면 빠져나가는 순간 기울기가 튕겨 돌아간다 —
	//	HoverBlend 가 0 으로 줄면서 자연스럽게 풀리게 두는 편이 낫다.
}

FReply UBGCardWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDragVisual)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D LocalSize = InGeometry.GetLocalSize();
	if (LocalSize.X > KINDA_SMALL_NUMBER && LocalSize.Y > KINDA_SMALL_NUMBER)
	{
		//	커서의 카드 안 위치를 -1 ~ +1 로. 카드 크기가 달라져도 같은 감도로 반응한다.
		const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		const FVector2D Raw(
			(Local.X / LocalSize.X - 0.5f) * 2.f,
			(Local.Y / LocalSize.Y - 0.5f) * 2.f);

		//	⚠️ 클램프 **전에** 판정해야 한다. 밖으로 얼마나 나갔는지가 판정의 전부인데
		//	   클램프하면 그 정보가 -1/+1 로 뭉개진다.
		UpdateHoverBounds(Raw);

		HoverLocalNormalized = FVector2D(
			FMath::Clamp(Raw.X, -1.f, 1.f),
			FMath::Clamp(Raw.Y, -1.f, 1.f));
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UBGCardWidget::UpdateHoverBounds(const FVector2D& RawLocal)
{
	if (!bUnhoverAtOriginalBounds)
	{
		return;
	}

	//	호버로 커진 배율. 아직 안 커졌으면 커진 영역과 원래 영역이 같아 볼 것이 없다.
	const float HoverFactor = FMath::Lerp(1.f, HoverScale, HoverBlend);
	if (HoverFactor <= 1.f + KINDA_SMALL_NUMBER)
	{
		return;
	}

	//	슬레이트의 로컬 좌표 -1~+1 은 **커진 카드 전체**를 덮는다(렌더 트랜스폼이
	//	AbsoluteToLocal 에 이미 반영돼 있다). 커지기 전 카드는 그 가운데 1/배율 만큼이다.
	//
	//	⚠️ 두 축의 기준이 다르다.
	//	   가로 — 커지기 전 카드 폭(1/배율). 부채꼴에서 카드가 겹치는 축이 가로뿐이라,
	//	          커진 카드가 옆 카드를 덮은 만큼은 놓아 줘야 커서를 옆으로 옮길 때 넘어간다.
	//	   세로 — 커진 카드 높이 그대로(1). 위아래로는 뺏어 갈 카드가 없어 좁힐 이유가 없고,
	//	          좁히면 눈에는 카드 위에 커서가 있는데 호버가 풀리는 띠가 생긴다
	//	          (카드가 커질수록 그 띠가 넓어져 위쪽 가장자리에서 특히 눈에 띄었다).
	//
	//	⚠️ 가로 기준은 배율이 줄어드는 동안에도 흔들리지 않는다. 커서의 절대 거리로 환산하면
	//	   양변이 같은 배율로 나뉘어 상쇄되므로, 경계는 언제나 원래 카드의 가장자리다.
	//	   (그래서 "나갔다 → 작아졌다 → 다시 들어왔다" 로 진동하지 않는다.)
	const float ExitLimitX  = 1.f / HoverFactor;
	const float EnterLimitX = ExitLimitX * (1.f - HoverExitDeadband);

	const float ExitLimitY  = 1.f;
	const float EnterLimitY = ExitLimitY * (1.f - HoverExitDeadband);

	//	사각형 판정이라 축마다 따로 본다 — 한 축이라도 벗어나면 밖이다.
	const float ReachX = FMath::Abs(RawLocal.X);
	const float ReachY = FMath::Abs(RawLocal.Y);

	if (bHovered)
	{
		if (ReachX > ExitLimitX || ReachY > ExitLimitY)
		{
			bHovered = false;
		}
	}
	else if (ReachX < EnterLimitX && ReachY < EnterLimitY)
	{
		//	커진 카드 안이라 마우스 이벤트는 계속 오는데 원래 크기 밖으로 나갔던 커서가
		//	되돌아온 경우다. 여기서 되살리지 않으면 카드를 벗어났다 다시 들어와도
		//	호버가 안 붙는다(슬레이트는 이미 Enter 를 보냈으므로 다시 안 보낸다).
		bHovered = true;
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

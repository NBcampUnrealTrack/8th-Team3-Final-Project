// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widget/BGHandWidget.h"
#include "UI/Widget/BGCardWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogBGUI, Log, All);

namespace
{
	//	손패를 떠나는 카드의 Z 순서. 손패 카드(0 ~ 장수)보다 확실히 크면 된다.
	constexpr int32 LeavingCardZOrder = 1000;

	//	덱에서 날아오는 카드의 Z 순서. 이미 놓인 카드 위를 지나가야 궤적이 가리지 않는다.
	constexpr int32 DealingCardZOrder = 500;

	//	마우스가 올라간 카드의 Z 순서. 부채꼴에서 겹친 이웃에 가리면 안 된다.
	constexpr int32 HoveredCardZOrder = 300;
}

void UBGHandWidget::ApplyCardAlignment(UBGCardWidget* CardWidget) const
{
	if (!CardWidget)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CardWidget->Slot))
	{
		CanvasSlot->SetAlignment(CardAlignment);
	}

	//	회전 축을 슬롯 정렬과 맞춘다. 어긋나면 부채꼴이 돌 때 카드가 궤도를 그리며 튄다.
	CardWidget->SetRenderTransformPivot(CardAlignment);
}

UCanvasPanel* UBGHandWidget::GetCardCanvas() const
{
	return Cast<UCanvasPanel>(Box_Cards);
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  손패 갱신 — 디프(diff)
//
//  ⚠️ 전부 지우고 다시 만들면 안 된다. RefreshHandWidget 은 손패가 그대로여도 불린다
//     (턴 변경·어트리뷰트 변경으로 bPlayable 만 바뀌는 경우). 매번 새로 만들면
//     카드 위젯의 수명이 몇 프레임이라 연출이 계속 처음으로 되감긴다.
// ══════════════════════════════════════════════════════════════════════════════════════

void UBGHandWidget::SetHand(const TArray<FBGCardView>& InCards)
{
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

	if (!GetCardCanvas())
	{
		SetHandLegacy(InCards);
		return;
	}

	//	InstanceId 로 기존 위젯을 찾는다. 덱 생성 시 1부터 부여되는 고유 키라
	//	같은 종류 카드가 여러 장이어도 정확히 짝지어진다.
	TMap<int32, UBGCardWidget*> Existing;
	Existing.Reserve(CardWidgets.Num());
	for (UBGCardWidget* CardWidget : CardWidgets)
	{
		if (CardWidget)
		{
			Existing.Add(CardWidget->GetInstanceId(), CardWidget);
		}
	}

	TArray<TObjectPtr<UBGCardWidget>> NewOrder;
	NewOrder.Reserve(InCards.Num());

	int32 DealOrder = 0;

	//	이번 갱신에서 새로 들어온 카드들. 총 장수를 뒤늦게 채워 주려고 모아 둔다
	//	(FinalizeDealBatch).
	TArray<UBGCardWidget*> DealtThisBatch;

	//	손패가 비어 있다가 채워지는 것은 판 시작 배분이다. 턴 드로우와 연출을 다르게
	//	하고 싶을 때 쓰라고 구분해 둔다(서버가 사유를 보내 주기 전까지의 근사치다).
	const bool bFirstHand = CardWidgets.IsEmpty();

	//	순차 배분에서 다음 카드가 출발할 시각. 앞 카드가 안착한 뒤에 시작한다.
	//	이미 날아오는 중인 카드가 있으면 그 뒤에 줄을 세워야 겹치지 않는다.
	float NextDealDelay = 0.f;
	if (bSequentialDeal)
	{
		for (const UBGCardWidget* InFlight : CardWidgets)
		{
			if (InFlight && InFlight->IsDealInProgress())
			{
				NextDealDelay += InFlight->GetDealFlightDuration() + SequentialDealGap;
			}
		}
	}
	for (const FBGCardView& Card : InCards)
	{
		if (UBGCardWidget** Found = Existing.Find(Card.InstanceId))
		{
			//	이미 있는 카드 — 데이터만 갈아끼운다. 위젯도, 진행 중인 연출도 그대로 산다.
			UBGCardWidget* CardWidget = *Found;
			CardWidget->SetCardView(Card);
			CardWidget->SetDragLocked(bInteractionLocked);

			Existing.Remove(Card.InstanceId);
			NewOrder.Add(CardWidget);
			continue;
		}

		//	이미 떠나는 중인 카드다. 드롭 직후 서버가 아직 손패에서 빼지 않은 상태에서
		//	다른 이유로 갱신이 오면(어트리뷰트 변경 등) 여기에 걸린다.
		//	새로 만들면 중앙에 하나, 손패에 하나 — 같은 카드가 둘 보인다.
		//
		//	⚠️ 서버가 사용을 거절했다면 연출이 끝나 위젯이 파괴된 뒤 다음 갱신에서
		//	   다시 만들어진다(덱에서 오는 것처럼 보인다). 클라가 bPlayable 로 미리
		//	   걸러 내므로 드문 경우고, 저절로 복구되는 쪽을 택했다.
		if (IsOutgoing(Card.InstanceId))
		{
			continue;
		}

		//	새로 들어온 카드 — 덱에서 날아온다.
		//	TODO(FX): 지금은 전부 덱에서 온 것으로 본다. 서버가 이동 사유
		//	          (드로우 / 훔치기 / 보상)를 이벤트로 보내 주면 출발점을 갈라야 한다.
		UBGCardWidget* CardWidget = CreateCardWidget(Card);
		if (!CardWidget)
		{
			continue;
		}

		//	순차면 앞 카드가 안착한 뒤에, 아니면 일정 간격으로 어긋나게.
		const float Delay = bSequentialDeal ? NextDealDelay : (DealOrder * DealStaggerSeconds);
		if (bSequentialDeal)
		{
			NextDealDelay += CardWidget->GetDealFlightDuration() + SequentialDealGap;
		}

		//	⚠️ TotalInBatch 는 아직 모른다 — 이 루프가 끝나야 몇 장이 새로 들어왔는지 안다.
		//	   아래에서 채워 넣는다. Reason 도 지금은 유추할 근거가 없어 Draw 고정이다.
		FBGCardDealContext Context;
		Context.OrderInBatch = DealOrder;
		Context.Reason = bFirstHand ? EBGCardGainReason::InitialHand : EBGCardGainReason::Draw;

		CardWidget->BeginDeal(GetDeckAnchor(), Delay, Context);
		DealtThisBatch.Add(CardWidget);
		++DealOrder;
		NewOrder.Add(CardWidget);
	}

	//	짝을 찾지 못하고 남은 것 = 이번 갱신에서 손패를 떠난 카드.
	for (const TPair<int32, UBGCardWidget*>& Leftover : Existing)
	{
		BeginExitFor(Leftover.Value);
	}

	CardWidgets = MoveTemp(NewOrder);

	FinalizeDealBatch(DealtThisBatch);
	ReleaseHeldCardsMissingFrom(InCards);

	//	패널 크기를 아직 모르는 첫 프레임이면 보간 없이 배치한다 —
	//	폴백 폭으로 잡았다가 실제 폭으로 고칠 때 카드가 눈에 띄게 튄다.
	RelayoutCards(/*bSnap=*/!bHasValidPanelSize);
}

void UBGHandWidget::FinalizeDealBatch(const TArray<UBGCardWidget*>& Batch)
{
	//	묶음 총 장수는 SetHand 루프가 끝나야 알 수 있다 — BeginDeal 시점에는 몇 장이
	//	들어올지 모른다. "마지막 한 장" 판별은 이 값 없이는 불가능하다.
	for (UBGCardWidget* Dealt : Batch)
	{
		Dealt->SetDealBatchTotal(Batch.Num());
		OnCardDealt.Broadcast(Dealt->GetInstanceId(), Dealt->GetDealContext());
	}
}

void UBGHandWidget::ReleaseHeldCardsMissingFrom(const TArray<FBGCardView>& InCards)
{
	//	중앙에 잡혀 있던 카드가 서버 손패에서 사라졌다 = 서버가 사용을 확정했다.
	//
	//	⚠️ 이건 **폴백** 경로다. 정식 신호는 판정 통지(NotifyCardResolved)이고, 그쪽이
	//	   등급까지 실어 온다. 여기까지 오는 것은 판정을 거치지 않는 장비 카드이거나
	//	   판정 통지보다 손패 갱신이 먼저 도착한 경우다.
	//
	//	⚠️ 손패 목록이 아니라 들어온 목록(InCards)과 비교해야 한다. 잡혀 있는 카드는
	//	   이미 CardWidgets 에서 빠져 있어 손패 목록만 봐서는 알 수 없다.
	//
	//	해제는 OnCardPlayResolved 를 쏘고, 그 끝은 BP 다 — 구독자가 무엇을 하든
	//	OutgoingWidgets 순회가 깨지지 않도록 대상을 먼저 모아 둔 뒤 해제한다.
	TArray<UBGCardWidget*> ToRelease;
	for (UBGCardWidget* Outgoing : OutgoingWidgets)
	{
		if (!Outgoing || !Outgoing->IsHoldingAtCenter())
		{
			continue;
		}

		const bool bStillInHand = InCards.ContainsByPredicate(
			[Id = Outgoing->GetInstanceId()](const FBGCardView& View) { return View.InstanceId == Id; });

		if (!bStillInHand)
		{
			ToRelease.Add(Outgoing);
		}
	}

	for (UBGCardWidget* Outgoing : ToRelease)
	{
		//	판정을 못 받은 해제다 — 등급도 Steps 도 지어낼 수 없으니 비워서 넘긴다.
		//	나머지 칸은 카드가 자기 데이터로 채운다.
		FBGCardPlayContext WithoutRoll;
		WithoutRoll.bResolvedWithoutRoll = true;

		ReleaseHeldCard(Outgoing, WithoutRoll);
	}
}

void UBGHandWidget::SetHandLegacy(const TArray<FBGCardView>& InCards)
{
	if (!bLegacyPanelWarned)
	{
		bLegacyPanelWarned = true;
		UE_LOG(LogBGUI, Warning,
			TEXT("%s: Box_Cards 가 CanvasPanel 이 아닙니다(%s) — 카드 연출이 꺼집니다. "
			     "WBP 에서 Box_Cards 를 CanvasPanel 로 바꾸면 정렬·등장·퇴장 연출이 켜집니다."),
			*GetName(), *Box_Cards->GetClass()->GetName());
	}

	ClearHand();

	CardWidgets.Reserve(InCards.Num());
	for (const FBGCardView& Card : InCards)
	{
		UBGCardWidget* CardWidget = CreateCardWidget(Card);
		if (CardWidget)
		{
			CardWidgets.Add(CardWidget);
		}
	}
}

UBGCardWidget* UBGHandWidget::CreateCardWidget(const FBGCardView& Card)
{
	UBGCardWidget* CardWidget = CreateWidget<UBGCardWidget>(this, CardWidgetClass);
	if (!CardWidget)
	{
		return nullptr;
	}

	// SetOwningHand 는 SetCardView 보다 먼저. 드래그 시작 시 핸드를 알아야 한다.
	CardWidget->SetOwningHand(this);
	CardWidget->SetCardView(Card);

	//	잠긴 동안 손패가 갱신될 수 있다(서버 복제와 대상 선택은 동시에 일어난다).
	//	새 위젯에도 잠금을 그대로 물려주지 않으면 갱신 한 번에 잠금이 풀린다.
	CardWidget->SetDragLocked(bInteractionLocked);

	// 'Slot' 은 UWidget 의 멤버명이므로 섀도잉을 피한다 (C4458 은 에러로 승격됨).
	UPanelSlot* AddedSlot = Box_Cards->AddChild(CardWidget);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(AddedSlot))
	{
		//	AutoSize 로 두면 카드 WBP 가 정한 크기를 그대로 쓴다.
		//	CardWidth 는 배치 계산용 값일 뿐 실제 크기를 강제하지 않는다.
		CanvasSlot->SetAutoSize(true);
	}
	else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(AddedSlot))
	{
		HBoxSlot->SetPadding(CardPadding);
		HBoxSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	ApplyCardAlignment(CardWidget);

	return CardWidget;
}

void UBGHandWidget::ClearHand()
{
	if (Box_Cards)
	{
		Box_Cards->ClearChildren();
	}
	CardWidgets.Reset();

	//	퇴장 중이던 카드도 같이 사라진다 — ClearChildren 이 이미 떼어냈다.
	OutgoingWidgets.Reset();
	PlayedMarks.Reset();
	HeldCards.Reset();
}

bool UBGHandWidget::RemoveCardByInstanceId(int32 InstanceId)
{
	for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
	{
		UBGCardWidget* CardWidget = CardWidgets[Index];
		if (!CardWidget || CardWidget->GetInstanceId() != InstanceId)
		{
			continue;
		}

		//	즉시 파괴하지 않고 퇴장 연출로 넘긴다. 손패 목록에서는 지금 빠지므로
		//	나머지 카드는 곧바로 중앙 재정렬된다.
		CardWidgets.RemoveAt(Index);
		BeginExitFor(CardWidget);
		RelayoutCards();
		return true;
	}
	return false;
}

void UBGHandWidget::MarkCardPlayed(int32 InstanceId)
{
	if (InstanceId != INDEX_NONE)
	{
		PlayedMarks.Add(InstanceId, PlayedMarkLifetime);
	}
}

void UBGHandWidget::BeginExitFor(UBGCardWidget* CardWidget)
{
	if (!CardWidget)
	{
		return;
	}

	//	캔버스가 아니면 연출할 자리가 없다 — 예전처럼 즉시 떼어낸다.
	if (!GetCardCanvas())
	{
		CardWidget->RemoveFromParent();
		return;
	}

	//	떠나는 카드는 손패보다 앞에 그린다. 중앙으로 올라가는 동안 다른 카드에
	//	가리면 무슨 카드를 냈는지 보이지 않는다.
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CardWidget->Slot))
	{
		CanvasSlot->SetZOrder(LeavingCardZOrder);
	}

	//	내가 낸 카드만 중앙에 올려 보여 준다. 남이 훔쳐 갔거나 버려진 카드까지
	//	중앙에 세우면 "내가 뭘 한 것" 처럼 읽혀서 오히려 헷갈린다.
	const int32 InstanceId = CardWidget->GetInstanceId();
	if (PlayedMarks.Remove(InstanceId) > 0)
	{
		CardWidget->BeginPlayExit(GetPlayZoneAnchor(), GetDiscardAnchor());
	}
	else
	{
		CardWidget->BeginExit(GetDiscardAnchor());
	}

	OutgoingWidgets.Add(CardWidget);
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

// ══════════════════════════════════════════════════════════════════════════════════════
//  배치 (Layout)
//
//  ComputeCardSlot 은 상태를 읽지 않는 순수 함수다 — (Index, Count) 만 보고 답을 낸다.
//  그래야 언제 불러도 같은 결과가 나오고, 연출/디버깅/미리보기에 그대로 재사용된다.
// ══════════════════════════════════════════════════════════════════════════════════════

float UBGHandWidget::GetLayoutWidth() const
{
	//	지오메트리는 첫 프레임에 0 이다. 그때는 폴백 폭으로 계산하고 Tick 에서 고친다.
	return bHasValidPanelSize ? CachedPanelSize.X : MaxHandWidth;
}

float UBGHandWidget::GetLayoutHeight() const
{
	return bHasValidPanelSize ? CachedPanelSize.Y : 0.f;
}

FBGCardSlotLayout UBGHandWidget::ComputeCardSlot(int32 Index, int32 Count) const
{
	FBGCardSlotLayout Out;
	if (Count <= 0 || Index < 0)
	{
		return Out;
	}

	const float PanelWidth = GetLayoutWidth();

	//	퍼질 수 있는 폭. 패널이 아무리 넓어도 MaxHandWidth 를 넘지 않는다.
	const float Available = FMath::Min(PanelWidth, MaxHandWidth);

	//	세로 기준선. 정렬 Y 를 그대로 패널 높이의 비율로 쓴다 —
	//	  0.5 → 카드 한가운데가 패널 세로 중앙에
	//	  1.0 → 카드 밑동이 패널 바닥에
	//	정렬과 배치를 같은 값으로 묶어야 둘이 어긋나 카드가 위/아래로 튀지 않는다.
	const float BaselineY = GetLayoutHeight() * CardAlignment.Y + VerticalOffset;

	//	오른쪽 카드가 위로 겹친다. 뒤에 온 카드가 앞에 보이는 편이 집기 쉽다.
	Out.ZOrder = Index;

	//	카드 중심 사이의 목표 거리. 일직선이든 부채꼴이든 이 값에서 출발한다 —
	//	모드를 바꿔도 간격 감각이 그대로 유지된다.
	const float DesiredStep = FMath::Max(CardWidth + CardSpacing, MinCardStep);

	// ══════════════════════════════════════════════════════════════════
	//  부채를 끈 경우 — 일직선 배치
	// ══════════════════════════════════════════════════════════════════
	if (!bUseFanLayout)
	{
		float Step = DesiredStep;
		if (Count > 1)
		{
			//	다 펼쳤을 때의 총 폭 = 중심 간격 × (장수-1) + 카드 한 장 폭.
			//	넘치면 간격을 줄여 겹치게 만든다(뱅 원작처럼 손패가 많아지면 겹친다).
			const float NeededWidth = Step * (Count - 1) + CardWidth;
			if (NeededWidth > Available)
			{
				//	하한이 없으면 손패가 많을 때 카드가 완전히 포개져 한 장처럼 보인다.
				Step = FMath::Max((Available - CardWidth) / static_cast<float>(Count - 1), MinCardStep);
			}
		}

		const float CentersSpan = (Count > 1) ? Step * (Count - 1) : 0.f;
		Out.Position = FVector2D((PanelWidth - CentersSpan) * 0.5f + Step * Index, BaselineY);
		Out.AngleDegrees = 0.f;
		return Out;
	}

	// ══════════════════════════════════════════════════════════════════
	//  부채꼴 — 원호 위에 놓고 각도를 접선에 맞춘다
	//
	//  회전 중심은 손패 아래로 FanRadius 만큼 내려간 지점이다. 모든 카드가
	//  그 점에서 같은 거리에 놓이므로, 위치와 각도가 하나의 곡선으로 맞물린다.
	//  (직선 위에 놓고 각도만 주면 바깥 카드가 좌우로 삐죽 튀어나온다.)
	// ══════════════════════════════════════════════════════════════════
	//
	//	각도를 직접 받지 않고 **간격에서 역산**한다. 각도를 받으면 반지름을 만질 때마다
	//	간격이 같이 변해서 둘을 동시에 맞춰야 한다 — 이러면 두 값이 독립적이 된다.
	//	  현의 길이가 간격이 되는 각도 : θ = asin(간격 / 반지름)
	float AngleStep = FMath::RadiansToDegrees(
		FMath::Asin(FMath::Clamp(DesiredStep / FanRadius, 0.f, 1.f)));

	if (Count > 1)
	{
		//	전체 각도 상한. 장수가 많아져도 부채가 무한정 펴지지 않는다.
		if (AngleStep * (Count - 1) > MaxTotalFanAngle)
		{
			AngleStep = MaxTotalFanAngle / static_cast<float>(Count - 1);
		}

		//	가로 폭 상한. 원호의 현(chord) 길이가 Available 을 넘지 않도록 각도를 줄인다.
		//	  현의 절반 = FanRadius × sin(전체각/2)
		const float SinHalfSpan = FMath::Clamp(Available * 0.5f / FanRadius, 0.f, 1.f);
		const float MaxStep = FMath::RadiansToDegrees(2.f * FMath::Asin(SinHalfSpan))
			/ static_cast<float>(Count - 1);

		AngleStep = FMath::Min(AngleStep, MaxStep);
	}

	//	가운데를 0 으로 좌우 대칭. 1장이면 0 이라 정중앙에 똑바로 선다.
	const float AngleDegrees = (static_cast<float>(Index) - (Count - 1) * 0.5f) * AngleStep;
	const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);

	//	회전 중심에서 FanRadius 만큼 떨어진 원 위의 점.
	//	  X : 중앙에서 좌우로  = R·sin θ
	//	  Y : 가운데가 가장 높고 양끝이 내려간다 = R·(1 - cos θ)
	Out.Position = FVector2D(
		PanelWidth * 0.5f + FanRadius * FMath::Sin(AngleRadians),
		BaselineY         + FanRadius * (1.f - FMath::Cos(AngleRadians)));

	//	각도를 원호의 접선에 맞춘다. 이게 위치와 같은 θ 라서 부채가 매끄럽게 이어진다.
	Out.AngleDegrees = AngleDegrees;

	return Out;
}

void UBGHandWidget::RelayoutCards(bool bSnap)
{
	if (!GetCardCanvas())
	{
		return;   // 구형 패널 — 배치는 패널이 알아서 한다.
	}

	const int32 Count = CardWidgets.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UBGCardWidget* CardWidget = CardWidgets[Index];
		if (!CardWidget)
		{
			continue;
		}

		const FBGCardSlotLayout SlotLayout = ComputeCardSlot(Index, Count);

		//	디테일 패널에서 정렬을 바꾸면 다음 갱신에 바로 반영되도록 매번 맞춘다.
		ApplyCardAlignment(CardWidget);

		CardWidget->SetLayoutTarget(SlotLayout.Position, SlotLayout.AngleDegrees);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CardWidget->Slot))
		{
			//	날아오는 중에는 이미 놓인 카드 위로 지나간다. 안착하면 제 순서로 돌아간다
			//	(마지막 한 장이 착지하는 프레임에 손패가 한 번 다시 배치한다).
			//	마우스가 올라간 카드도 위로 올린다 — 부채꼴에서 이웃에 반쯤 가리면
			//	떠오르는 연출이 보이지 않는다.
			const int32 ZOrder =
				CardWidget->IsDealInProgress() ? DealingCardZOrder :
				CardWidget->IsHoverActive()    ? HoveredCardZOrder :
				                                 SlotLayout.ZOrder;
			CanvasSlot->SetZOrder(ZOrder);
		}

		if (bSnap)
		{
			CardWidget->SnapToLayoutTarget();
		}
	}
}

void UBGHandWidget::SetDeckAnchor(const FVector2D& InLocalPosition)
{
	DeckAnchor = InLocalPosition;
}

void UBGHandWidget::SetDiscardAnchor(const FVector2D& InLocalPosition)
{
	DiscardAnchor = InLocalPosition;
}

void UBGHandWidget::SetPlayZoneAnchor(const FVector2D& InLocalPosition)
{
	PlayZoneAnchor = InLocalPosition;
}

FVector2D UBGHandWidget::GetDeckAnchor() const
{
	//	폴백은 패널 바닥을 기준으로 잡는다 — 손패와 같은 높이에서 출발해야
	//	덱 위젯이 없어도 어색하지 않다.
	return DeckAnchor.Get(FVector2D(DeckAnchorFallback.X, GetLayoutHeight() + DeckAnchorFallback.Y));
}

FVector2D UBGHandWidget::GetDiscardAnchor() const
{
	return DiscardAnchor.Get(FVector2D(
		GetLayoutWidth() * 0.5f + DiscardAnchorFallback.X,
		GetLayoutHeight() + DiscardAnchorFallback.Y));
}

FVector2D UBGHandWidget::GetPlayZoneAnchor() const
{
	return PlayZoneAnchor.Get(FVector2D(
		GetLayoutWidth() * 0.5f + PlayZoneAnchorFallback.X,
		GetLayoutHeight() + PlayZoneAnchorFallback.Y));
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  틱 — 카드들의 보간을 여기서 한 번에 돌린다.
//
//  카드가 각자 틱하면 순서가 보장되지 않아, 손패가 재정렬되는 프레임에 어떤 카드는
//  옛 목표로 한 프레임 움직인다. 틱 지점을 하나로 모으면 그 문제가 사라진다.
// ══════════════════════════════════════════════════════════════════════════════════════

void UBGHandWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UCanvasPanel* Canvas = GetCardCanvas();
	if (!Canvas)
	{
		return;
	}

	//	패널 크기가 바뀌면(첫 프레임 / 해상도 변경 / 레이아웃 변경) 자리를 다시 잡는다.
	const FVector2D PanelSize = Canvas->GetCachedGeometry().GetLocalSize();
	if (PanelSize.X > 1.f && !PanelSize.Equals(CachedPanelSize, 1.f))
	{
		const bool bFirstValidSize = !bHasValidPanelSize;

		CachedPanelSize    = PanelSize;
		bHasValidPanelSize = true;

		//	처음 유효해진 프레임에는 보간 없이 배치한다 —
		//	폴백 폭으로 잡아 둔 자리에서 실제 자리로 미끄러지는 것이 보이면 안 된다.
		RelayoutCards(/*bSnap=*/bFirstValidSize);
	}

	bool bAnyDealing = false;
	int32 HoveredInstanceId = INDEX_NONE;
	for (UBGCardWidget* CardWidget : CardWidgets)
	{
		if (CardWidget)
		{
			CardWidget->TickMotion(InDeltaTime);
			bAnyDealing |= CardWidget->IsDealInProgress();

			if (CardWidget->IsHoverActive())
			{
				HoveredInstanceId = CardWidget->GetInstanceId();
			}
		}
	}

	//	Z 순서는 상태가 바뀌는 순간에만 다시 잡는다. 매 프레임 SetZOrder 를 부르면
	//	슬레이트가 계속 레이아웃을 무효화한다.
	//	  배분이 끝났을 때  → 올려 뒀던 순서를 제자리로
	//	  호버가 옮겨갔을 때 → 새로 올라온 카드를 위로
	const bool bDealingEnded = bWasDealing && !bAnyDealing;
	const bool bHoverChanged = HoveredInstanceId != LastHoveredInstanceId;

	if (bDealingEnded || bHoverChanged)
	{
		RelayoutCards();
	}

	bWasDealing = bAnyDealing;
	LastHoveredInstanceId = HoveredInstanceId;

	//	"내가 냈다" 표시 만료. 서버가 거절한 카드의 표시가 남아 있으면
	//	나중에 그 카드가 다른 사유로 빠질 때 엉뚱하게 중앙 연출이 나온다.
	for (auto MarkIt = PlayedMarks.CreateIterator(); MarkIt; ++MarkIt)
	{
		MarkIt.Value() -= InDeltaTime;
		if (MarkIt.Value() <= 0.f)
		{
			MarkIt.RemoveCurrent();
		}
	}

	//	퇴장 완료된 카드를 걷어낸다. 뒤에서부터 지워야 인덱스가 밀리지 않는다.
	TArray<int32> TimedOutIds;
	for (int32 Index = OutgoingWidgets.Num() - 1; Index >= 0; --Index)
	{
		UBGCardWidget* CardWidget = OutgoingWidgets[Index];
		if (!CardWidget)
		{
			OutgoingWidgets.RemoveAt(Index);
			continue;
		}

		CardWidget->TickMotion(InDeltaTime);

		if (CardWidget->IsExitFinished())
		{
			const int32 FinishedId = CardWidget->GetInstanceId();
			HeldCards.Remove(FinishedId);

			CardWidget->RemoveFromParent();
			OutgoingWidgets.RemoveAt(Index);
			continue;
		}

		//	안전장치 — 확정도 취소도 오지 않은 카드를 영원히 중앙에 두지 않는다.
		if (CardWidget->IsHoldingAtCenter())
		{
			FHeldCardRecord& Record = HeldCards.FindOrAdd(CardWidget->GetInstanceId());
			Record.Elapsed += InDeltaTime;
			if (Record.Elapsed >= HoldTimeoutSeconds)
			{
				TimedOutIds.Add(CardWidget->GetInstanceId());
			}
		}
	}

	//	되돌리기는 OutgoingWidgets 를 건드리므로 순회가 끝난 뒤에 한다.
	for (const int32 TimedOutId : TimedOutIds)
	{
		UE_LOG(LogBGUI, Warning,
			TEXT("%s: 카드 #%d 가 중앙에서 %.0f초 동안 확정되지 않아 손패로 되돌립니다."),
			*GetName(), TimedOutId, HoldTimeoutSeconds);

		ReturnHeldCardToHand(TimedOutId);   // 기록 정리도 여기서 함께 한다
	}
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

	//	손패에서 빠질 때 중앙 연출을 태우기 위한 표시.
	//	⚠️ 서버가 거절하면 카드는 손패에 남고 표시만 시간이 지나 만료된다.
	//	   Discard 페이즈의 버리기도 이 경로를 타므로 버린 카드도 중앙에 올라간다 —
	//	   구분이 필요해지면 PlayerController 가 표시를 직접 관리하면 된다.
	MarkCardPlayed(Card.InstanceId);

	// 여기서 카드를 제거하지 않는다. 손패에서 빠지는 시점은 서버가 결정한다.
	OnCardPlayRequested.Broadcast(Card);
}

void UBGHandWidget::NotifyCardDroppedOnPlayZone(
	const FBGCardView& Card, const FGeometry& ZoneGeometry, const FVector2D& DropScreenPosition)
{
	if (!Card.IsValid())
	{
		return;
	}

	UCanvasPanel* Canvas = GetCardCanvas();

	//	대상이 필요하든 아니든 일단 중앙으로 보낸다. 대상을 고르는 동안에는
	//	중앙에 잡혀 서 있다가(ReleaseHold 대기), 서버가 확정하면 그때 사라진다.
	if (Canvas)
	{
		const FGeometry& CanvasGeometry = Canvas->GetCachedGeometry();

		//	플레이 존의 중앙이 곧 카드가 머물 자리다. 매번 갱신하므로 존이 움직여도 따라간다.
		const FVector2D ZoneCenterAbsolute = ZoneGeometry.LocalToAbsolute(ZoneGeometry.GetLocalSize() * 0.5f);
		SetPlayZoneAnchor(CanvasGeometry.AbsoluteToLocal(ZoneCenterAbsolute));

		//	커서를 놓은 지점에서 이어받는다. 드래그 비주얼이 있던 자리와 같아야 끊겨 보이지 않는다.
		BeginPlayPresentationAt(Card.InstanceId, CanvasGeometry.AbsoluteToLocal(DropScreenPosition));
	}

	NotifyCardPlayRequested(Card);
}

bool UBGHandWidget::BeginPlayPresentationAt(int32 InstanceId, const FVector2D& LocalStartPosition)
{
	for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
	{
		UBGCardWidget* CardWidget = CardWidgets[Index];
		if (!CardWidget || CardWidget->GetInstanceId() != InstanceId)
		{
			continue;
		}

		//	손패 목록에서 지금 뺀다 — 나머지 카드가 곧바로 중앙 재정렬된다.
		//	되돌릴 때 원래 자리로 넣기 위해 위치를 기억한다.
		HeldCards.Add(InstanceId, FHeldCardRecord{ Index, 0.f });
		CardWidgets.RemoveAt(Index);

		//	드롭 지점으로 순간이동시킨 뒤 거기서 연출을 시작한다.
		CardWidget->SetMotionPosition(LocalStartPosition);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CardWidget->Slot))
		{
			CanvasSlot->SetZOrder(LeavingCardZOrder);
		}

		CardWidget->BeginPlayExit(GetPlayZoneAnchor(), GetDiscardAnchor());
		OutgoingWidgets.Add(CardWidget);

		RelayoutCards();
		return true;
	}

	return false;
}

bool UBGHandWidget::ReturnHeldCardToHand(int32 InstanceId)
{
	for (int32 Index = 0; Index < OutgoingWidgets.Num(); ++Index)
	{
		UBGCardWidget* CardWidget = OutgoingWidgets[Index];
		if (!CardWidget || CardWidget->GetInstanceId() != InstanceId)
		{
			continue;
		}

		if (!CardWidget->ReturnToHand())
		{
			return false;   // 이미 사라지는 중이라 되돌릴 수 없다.
		}

		OutgoingWidgets.RemoveAt(Index);

		//	빠졌던 자리로 되돌린다. 모르면 끝에 붙인다 — 다음 갱신에서 순서가 바로잡힌다.
		int32 InsertAt = CardWidgets.Num();
		if (const FHeldCardRecord* Record = HeldCards.Find(InstanceId))
		{
			InsertAt = FMath::Clamp(Record->OriginalIndex, 0, CardWidgets.Num());
		}
		HeldCards.Remove(InstanceId);

		CardWidgets.Insert(CardWidget, InsertAt);

		//	떠날 때 올려 둔 Z 순서를 되돌린다. RelayoutCards 가 인덱스대로 다시 잡는다.
		CardWidget->SetDragLocked(bInteractionLocked);
		RelayoutCards();
		return true;
	}

	return false;
}

UBGCardWidget* UBGHandWidget::FindHeldCard(int32 InstanceId) const
{
	for (UBGCardWidget* CardWidget : OutgoingWidgets)
	{
		if (!CardWidget || !CardWidget->IsHoldingAtCenter())
		{
			continue;
		}

		if (InstanceId == INDEX_NONE || CardWidget->GetInstanceId() == InstanceId)
		{
			return CardWidget;
		}
	}
	return nullptr;
}

void UBGHandWidget::ReleaseHeldCard(UBGCardWidget* CardWidget, const FBGCardPlayContext& Context)
{
	if (!CardWidget)
	{
		return;
	}

	//	빈 칸(연출 원형)은 카드가 스스로 채워 돌려준다. 그 결과를 관찰자에게 흘린다 —
	//	카드가 받은 것과 바깥이 받는 것이 어긋나면 추적이 매우 어렵다.
	const FBGCardPlayContext Resolved = CardWidget->ReleaseHold(Context);

	HeldCards.Remove(CardWidget->GetInstanceId());
	OnCardPlayResolved.Broadcast(CardWidget->GetInstanceId(), Resolved);
}

void UBGHandWidget::NotifyCardResolved(const FBGCardPlayContext& Context, int32 InstanceId)
{
	ReleaseHeldCard(FindHeldCard(InstanceId), Context);
}

bool UBGHandWidget::IsOutgoing(int32 InstanceId) const
{
	for (const UBGCardWidget* CardWidget : OutgoingWidgets)
	{
		if (CardWidget && CardWidget->GetInstanceId() == InstanceId)
		{
			return true;
		}
	}
	return false;
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

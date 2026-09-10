// UBGHandWidget — 손패 갱신(디프) · 카드 위젯 수명 · 틱.
//
// ── 이 클래스는 세 개의 .cpp 로 나뉘어 있다 ────────────────────────────────
//   BGHandWidget.cpp        (여기) 갱신 · 생성/제거 · 틱
//   BGHandWidget_Layout.cpp       배치 계산 · 앵커
//   BGHandWidget_Play.cpp         사용 흐름(중앙·대기·해제) · 연출 큐 중계
//
// 한 클래스를 나눈 것이지 세 클래스가 아니다 — 헤더는 하나이고 멤버도 공유한다.
// 카드 위젯(BGCardWidget_*.cpp)과 같은 방식이다.
//
// ⚠️ 새 함수를 넣을 때 파일을 고르는 기준: **그 함수가 무엇을 만지는가**.
//    CardWidgets 목록이면 여기, 좌표·앵커면 Layout, OutgoingWidgets/연출이면 Play.

#include "UI/Widget/BGHandWidget.h"
#include "UI/Widget/BGCardWidget.h"
#include "UI/Widget/BGHandWidgetInternal.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"

//	⚠️ 로그 카테고리의 정의는 여기 한 곳뿐이다. 나머지 .cpp 는 내부 헤더로 선언만 본다.
DEFINE_LOG_CATEGORY(LogBGUI);

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

	//	연출 신호를 밖으로 중계한다. 해제는 카드가 파괴되면서 함께 사라진다.
	CardWidget->OnPlayFXStarted.AddDynamic(this, &UBGHandWidget::HandleCardPlayFXStarted);

	//	잠긴 동안 손패가 갱신될 수 있다(서버 복제와 대상 선택은 동시에 일어난다).
	//	새 위젯에도 잠금을 그대로 물려주지 않으면 갱신 한 번에 잠금이 풀린다.
	CardWidget->SetDragLocked(bInteractionLocked);

	// 'Slot' 은 UWidget 의 멤버명이므로 섀도잉을 피한다 (C4458 은 에러로 승격됨).
	UPanelSlot* AddedSlot = Box_Cards->AddChild(CardWidget);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(AddedSlot))
	{
		//	AutoSize 로 두면 카드 WBP 가 정한 크기를 그대로 쓴다.
		//	CardWidth 는 배치 계산용 값일 뿐 실제 크기를 강제하지 않는다.
		//
		//	⚠️ 그래서 카드 **안의 내용물**이 크기를 밀어낼 수 있다. 줄바꿈이 꺼진 설명
		//	   텍스트 한 줄이 카드를 가로로 늘리면 텍스트가 긴 카드만 넓어진다 —
		//	   카드 폭이 제각각으로 보이면 WBP 의 Auto Wrap Text 부터 확인할 것.
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
	PendingResolutionRemaining = 0.f;
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
//  틱 — 카드들의 보간을 여기서 한 번에 돌린다.
//
//  카드가 각자 틱하면 순서가 보장되지 않아, 손패가 재정렬되는 프레임에 어떤 카드는
//  옛 목표로 한 프레임 움직인다. 틱 지점을 하나로 모으면 그 문제가 사라진다.
// ══════════════════════════════════════════════════════════════════════════════════════

//	한 프레임에 흘려보낼 시간의 상한(초).
//
//	포커스를 잃으면 슬레이트가 틱을 줄이거나 아예 멈춘다. 돌아오는 순간 그동안 쌓인
//	시간이 **한 프레임에 몰려서** 들어오는데, 그대로 흘리면 Deal.Elapsed 가 통째로 뛰어
//	카드가 호 중간을 건너뛰고, InterpTo 계열은 큰 델타에서 목표로 스냅해 궤적이 무너진다.
//
//	⚠️ 자르는 것은 **여기 한 곳뿐**이다. 카드 이동 · "내가 냈다" 표시 만료 · 중앙 대기
//	   타임아웃이 전부 이 델타를 쓰므로, 내려보내기 전에 잘라야 시간 기준이 하나로 맞는다.
//	   카드 쪽에서 각자 자르면 손패의 타이머만 실시간으로 흘러 서로 어긋난다.
//
//	0.1 = 10 FPS. 그보다 느리면 시간이 느리게 흐른다 — 이미 정상 상태가 아닌 구간이고,
//	연출이 튀는 것보다 느려지는 편이 낫다.
static float GBGCardMaxMotionDelta = 0.1f;
static FAutoConsoleVariableRef CVarBGCardMaxMotionDelta(
	TEXT("BG.Card.MaxMotionDelta"),
	GBGCardMaxMotionDelta,
	TEXT("손패 연출이 한 프레임에 흘려보낼 시간의 상한(초). 0 이면 자르지 않는다. 기본 0.1."),
	ECVF_Default);

void UBGHandWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	//	아래는 전부 이 값을 쓴다. InDeltaTime 을 직접 쓰는 곳이 하나라도 남으면
	//	그것만 실시간으로 흘러 다른 것들과 어긋난다.
	const float DeltaTime = GBGCardMaxMotionDelta > 0.f
		? FMath::Min(InDeltaTime, GBGCardMaxMotionDelta)
		: InDeltaTime;

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
	int32 RevealedCount = 0;
	for (UBGCardWidget* CardWidget : CardWidgets)
	{
		if (CardWidget)
		{
			CardWidget->TickMotion(DeltaTime);
			bAnyDealing |= CardWidget->IsDealInProgress();

			//	TickMotion 안에서 stagger 대기가 풀린다. 그래서 세는 것은 그 뒤여야
			//	이번 프레임에 나타난 카드가 바로 반영된다.
			if (CardWidget->GetMotionState() != EBGCardMotionState::Pending)
			{
				++RevealedCount;
			}

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
	//	  한 장 더 나타났을 때 → 그만큼 손패를 더 벌린다(부채가 펼쳐진다)
	const bool bDealingEnded  = bWasDealing && !bAnyDealing;
	const bool bHoverChanged  = HoveredInstanceId != LastHoveredInstanceId;
	const bool bRevealChanged = bProgressiveFanOpen && RevealedCount != LastRevealedCount;

	if (bDealingEnded || bHoverChanged || bRevealChanged)
	{
		RelayoutCards();
	}

	bWasDealing = bAnyDealing;
	LastHoveredInstanceId = HoveredInstanceId;
	LastRevealedCount = RevealedCount;

	//	"내가 냈다" 표시 만료. 서버가 거절한 카드의 표시가 남아 있으면
	//	나중에 그 카드가 다른 사유로 빠질 때 엉뚱하게 중앙 연출이 나온다.
	for (auto MarkIt = PlayedMarks.CreateIterator(); MarkIt; ++MarkIt)
	{
		MarkIt.Value() -= DeltaTime;
		if (MarkIt.Value() <= 0.f)
		{
			MarkIt.RemoveCurrent();
		}
	}

	//	주인을 못 찾은 판정 통지도 같은 이유로 만료시킨다 — 남겨 두면 한참 뒤에
	//	낸 카드가 남의 판정 결과로 연출된다.
	PendingResolutionRemaining = FMath::Max(PendingResolutionRemaining - DeltaTime, 0.f);

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

		CardWidget->TickMotion(DeltaTime);

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
			Record.Elapsed += DeltaTime;
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

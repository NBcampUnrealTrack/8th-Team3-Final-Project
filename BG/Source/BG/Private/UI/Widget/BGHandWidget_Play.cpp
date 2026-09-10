// UBGHandWidget — 카드 사용 흐름 (중앙으로 · 대기 · 해제) 과 연출 큐 중계.
//
// 손패에서 빠진 카드는 여기서부터 손패 소속이 아니다 — OutgoingWidgets 가 맡고,
// 퇴장이 끝나면 틱이 걷어간다.

#include "UI/Widget/BGHandWidget.h"
#include "UI/Widget/BGCardWidget.h"
#include "UI/Widget/BGHandWidgetInternal.h"
#include "UI/Presentation/BGPresentationDirector.h"
#include "Core/BGPlayerController.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

void UBGHandWidget::MarkCardPlayed(int32 InstanceId)
{
	if (InstanceId != INDEX_NONE)
	{
		PlayedMarks.Add(InstanceId, PlayedMarkLifetime);
	}
}

void UBGHandWidget::ClearPlayedMark(int32 InstanceId)
{
	PlayedMarks.Remove(InstanceId);
}

void UBGHandWidget::ShowSpectatorCard(const FBGCardView& Card, const FBGCardPlayContext& Context)
{
	//	연출을 태울 자리가 없다(구형 HorizontalBox 경로). 조용히 넘어간다 —
	//	관전 연출이 없다고 판이 멈출 이유는 없다.
	UCanvasPanel* Canvas = GetCardCanvas();
	if (!Canvas)
	{
		return;
	}

	UBGCardWidget* CardWidget = CreateCardWidget(Card);
	if (!CardWidget)
	{
		return;
	}

	//	손패 위로 올라와야 한다 — 남이 낸 카드가 내 카드 뒤에 가리면 아무 의미가 없다.
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CardWidget->Slot))
	{
		CanvasSlot->SetZOrder(BGHand::LeavingCardZOrder);
	}

	const FVector2D Center = GetPlayZoneAnchor();
	CardWidget->SetMotionPosition(Center + SpectatorCardEntryOffset);

	CardWidget->BeginPlayExit(Center, GetDiscardAnchor());

	//	⚠️ CardWidgets 에는 넣지 않는다. 손패 배치에 끼면 부채꼴이 한 장만큼 벌어진다.
	//	   OutgoingWidgets 에만 넣어 두면 퇴장이 끝날 때 틱이 알아서 걷어간다.
	OutgoingWidgets.Add(CardWidget);

	//	판정은 이미 끝난 채로 온다. 지금 풀어 주면 카드가 중앙에 **도착하는 순간**
	//	판정 연출이 터진다(날아가는 중에는 밀어 두었다가 도착 시 소비한다).
	CardWidget->ReleaseHold(Context);
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
		CanvasSlot->SetZOrder(BGHand::LeavingCardZOrder);
	}

	//	내가 낸 카드만 중앙에 올려 보여 준다. 남이 훔쳐 갔거나 버려진 카드까지
	//	중앙에 세우면 "내가 뭘 한 것" 처럼 읽혀서 오히려 헷갈린다.
	const int32 InstanceId = CardWidget->GetInstanceId();
	if (PlayedMarks.Remove(InstanceId) > 0)
	{
		CardWidget->BeginPlayExit(GetPlayZoneAnchor(), GetDiscardAnchor());

		//	판정이 먼저 도착해 있었다면 여기서 바로 푼다 — 중앙에 잡아 둘 이유가 없다.
		//	대상 카드는 좌석을 고르는 사이에 판정이 끝나 있는 것이 보통이라 이 길로 온다.
		//	⚠️ 통지에 InstanceId 가 없을 때만(INDEX_NONE) 아무 카드에나 붙인다.
		//	   지정돼 있는데 다른 카드에 붙이면 남의 판정으로 연출된다.
		if (PendingResolutionRemaining > 0.f
			&& (PendingResolutionId == INDEX_NONE || PendingResolutionId == InstanceId))
		{
			const FBGCardPlayContext Context = PendingResolution;
			PendingResolutionRemaining = 0.f;
			ReleaseHeldCard(CardWidget, Context);
		}
	}
	else
	{
		CardWidget->BeginExit(GetDiscardAnchor());
	}

	OutgoingWidgets.Add(CardWidget);
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
	const FBGCardView& Card, const FGeometry& /*ZoneGeometry*/, const FVector2D& DropScreenPosition)
{
	if (!Card.IsValid())
	{
		return;
	}

	UCanvasPanel* Canvas = GetCardCanvas();

	//	⚠️ 여기서 앵커를 박아 두지 않는다. 드롭한 사람만 존의 지오메트리를 알던 탓에
	//	   남이 낸 카드(ShowSpectatorCard)는 폴백 자리에 서서 화면마다 위치가 달랐다 —
	//	   지금은 GetPlayZoneAnchor 가 존의 중앙을 직접 읽으므로 두 경로가 같은 자리다.
	//	   (드롭 지오메트리는 그 존의 캐시 지오메트리와 같은 것이라 잃는 정보가 없다.)

	//	대상 지정 카드(뱅 등)도 카드를 세운다 — 좌석을 고르는 동안 무엇을 내는 중인지
	//	화면에 남아 있어야 한다. 다만 아직 확정된 카드가 아니므로 낸 카드와 같은 자리에
	//	두지 않는다: GetTargetingAnchor 가 플레이 존을 비켜선 자리를 준다.
	//	취소되면 HandleTargetSelectionCancelled → ReturnHeldCardToHand 로 손패에 돌아간다.
	//
	//	bPresentCardWhileTargeting 을 끄면 예전처럼 손패에 그대로 두고, 서버가 확정해
	//	손패에서 빠질 때 BeginExitFor 가 중앙 연출과 디졸브를 태운다.
	const bool bTargeting = Card.bNeedsTarget;
	if (Canvas && (!bTargeting || bPresentCardWhileTargeting))
	{
		//	커서를 놓은 지점에서 이어받는다. 드래그 비주얼이 있던 자리와 같아야 끊겨 보이지 않는다.
		BeginPlayPresentationAt(Card.InstanceId,
			Canvas->GetCachedGeometry().AbsoluteToLocal(DropScreenPosition), bTargeting);
	}

	NotifyCardPlayRequested(Card);
}

bool UBGHandWidget::BeginPlayPresentationAt(int32 InstanceId, const FVector2D& LocalStartPosition,
	bool bTargeting)
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
			CanvasSlot->SetZOrder(BGHand::LeavingCardZOrder);
		}

		//	⚠️ 대상 선택 중에는 다른 자리에 세운다. 여기서 GetPlayZoneAnchor 를 쓰면 아직
		//	   내지도 않은 카드가 판 한복판에 서서, 좌석을 고르는 동안 보드를 가린다.
		CardWidget->BeginPlayExit(
			bTargeting ? GetTargetingAnchor() : GetPlayZoneAnchor(),
			GetDiscardAnchor());

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

TArray<int32> UBGHandWidget::GetHeldCardIds() const
{
	//	사라지는 중인 카드는 세지 않는다. 이미 확정돼 날아가는 것이라 되돌릴 대상이 아니다.
	TArray<int32> HeldIds;
	for (const UBGCardWidget* CardWidget : OutgoingWidgets)
	{
		if (CardWidget && CardWidget->IsHoldingAtCenter())
		{
			HeldIds.Add(CardWidget->GetInstanceId());
		}
	}

	return HeldIds;
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

	//	⚠️ ReleaseHold 보다 **먼저**다. 대상 선택 중인 카드는 보드를 가리지 않으려고
	//	   비켜선 자리에 서 있는데, 확정 뒤에도 거기 있으면 낸 사람 화면에서만 카드가
	//	   옆에 서고 남의 화면(ShowSpectatorCard)에서는 중앙에 선다.
	CardWidget->MoveHoldTo(GetPlayZoneAnchor());

	//	빈 칸(연출 원형)은 카드가 스스로 채워 돌려준다. 그 결과를 관찰자에게 흘린다 —
	//	카드가 받은 것과 바깥이 받는 것이 어긋나면 추적이 매우 어렵다.
	const FBGCardPlayContext Resolved = CardWidget->ReleaseHold(Context);

	HeldCards.Remove(CardWidget->GetInstanceId());
	OnCardPlayResolved.Broadcast(CardWidget->GetInstanceId(), Resolved);
}

void UBGHandWidget::HandleCardPlayFXStarted(UBGCardWidget* Card, const FBGCardPlayContext& Context)
{
	//	디렉터가 연출의 주인이다. 여기서는 "카드가 중앙에 서서 터졌다"만 알린다.
	//	⚠️ 카드를 **큐와 같은 순서로** 쌓는다. 한 장짜리 슬롯으로 두면 카드 두 장이
	//	   겹칠 때 앞 카드를 잃어버려, 그 카드가 종료 신호를 못 받고 중앙에 박힌다.
	if (UBGPresentationDirector* Director = EnsureDirectorBound())
	{
		PresentingCards.Add(Card);
		Director->EnqueueCardPlay(Context);
	}
}

UBGPresentationDirector* UBGHandWidget::EnsureDirectorBound()
{
	UBGPresentationDirector* Director = UBGPresentationDirector::Get(this);
	if (!Director)
	{
		return nullptr;   // 아직 PlayerController 가 붙기 전이다.
	}

	//	연출 종료는 디렉터가 정한다 — 손패는 그 결정을 카드에 전달만 한다.
	//	AddUniqueDynamic 이라 여러 번 불려도 구독은 하나다.
	Director->OnPresentationFinished.AddUniqueDynamic(
		this, &UBGHandWidget::HandlePresentationFinished);

	return Director;
}

void UBGHandWidget::HandlePresentationFinished(const FBGPresentationRequest& Request)
{
	//	디렉터는 넣은 순서대로 끝낸다 — 맨 앞이 이번에 끝난 연출의 카드다.
	//	이미 파괴됐으면 nullptr 이고, 그때는 놓아 줄 것도 없다.
	UBGCardWidget* Card = nullptr;
	if (!PresentingCards.IsEmpty())
	{
		Card = PresentingCards[0].Get();
		PresentingCards.RemoveAt(0);
	}

	//	중앙에 서 있던 카드를 놓아 준다(퇴장 · 글로우 종료).
	if (Card)
	{
		Card->NotifyPlayFXFinished();
	}
}



void UBGHandWidget::NotifyCardResolved(const FBGCardPlayContext& Context, int32 InstanceId)
{
	if (UBGCardWidget* HeldCard = FindHeldCard(InstanceId))
	{
		ReleaseHeldCard(HeldCard, Context);
		return;
	}

	//	아직 중앙에 올라오지 않았다 — 대상 카드는 서버가 확정한 뒤에야 손패에서 빠지므로
	//	판정 통지가 손패 복제를 앞지를 수 있다. 잠깐 들고 있다가 연출이 시작될 때 푼다.
	PendingResolution          = Context;
	PendingResolutionId        = InstanceId;
	PendingResolutionRemaining = PlayedMarkLifetime;
}

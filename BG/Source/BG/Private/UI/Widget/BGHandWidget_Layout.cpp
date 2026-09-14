// UBGHandWidget — 배치 계산과 기준점(앵커).
//
// ComputeCardSlot 은 상태를 읽지 않는 순수 함수다 — (Index, Count) 만 보고 답을 낸다.
// 그래야 언제 불러도 같은 결과가 나오고, 연출·디버깅·미리보기에 그대로 재사용된다.

#include "UI/Widget/BGHandWidget.h"
#include "UI/Widget/BGCardWidget.h"
#include "UI/Widget/BGPlayZoneWidget.h"
#include "UI/Widget/BGHandWidgetInternal.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

float UBGHandWidget::GetLayoutWidth() const
{
	//	지오메트리는 첫 프레임에 0 이다. 그때는 폴백 폭으로 계산하고 Tick 에서 고친다.
	return bHasValidPanelSize ? CachedPanelSize.X : MaxHandWidth;
}

float UBGHandWidget::GetLayoutHeight() const
{
	return bHasValidPanelSize ? CachedPanelSize.Y : 0.f;
}

float UBGHandWidget::GetEffectiveCardWidth() const
{
	if (bMeasureCardWidth)
	{
		//	손패 카드는 전부 같은 WBP 라 한 장만 재면 된다. 아직 레이아웃 전이면 0 이 나오므로
		//	유효한 값이 나오는 첫 장을 쓴다(퇴장 중인 카드는 크기가 줄어드는 중이라 건너뛴다).
		for (const UBGCardWidget* CardWidget : CardWidgets)
		{
			if (!CardWidget || CardWidget->IsLeavingHand())
			{
				continue;
			}

			const float Measured = static_cast<float>(CardWidget->GetDesiredSize().X);
			if (Measured > KINDA_SMALL_NUMBER)
			{
				return Measured;
			}
		}
	}

	//	첫 프레임이라 아직 잰 값이 없다. 눈에 띄게 틀리지만 않으면 한 프레임 뒤 교정된다.
	return CardWidth;
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
	//
	//	폭은 실제 위젯에서 잰다(GetEffectiveCardWidth). 겹침을 비율로 잡으므로
	//	카드 크기를 바꿔도 "30% 가려진다" 는 감각이 그대로 유지된다.
	const float EffectiveWidth = GetEffectiveCardWidth();
	const float DesiredStep = FMath::Max(EffectiveWidth * (1.f - CardOverlapRatio), MinCardStep);

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
			const float NeededWidth = Step * (Count - 1) + EffectiveWidth;
			if (NeededWidth > Available)
			{
				//	하한이 없으면 손패가 많을 때 카드가 완전히 포개져 한 장처럼 보인다.
				Step = FMath::Max((Available - EffectiveWidth) / static_cast<float>(Count - 1), MinCardStep);
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
	//	다만 접선 각도는 카드 폭과 장수에서 역산되므로 손패가 길어지면 조용히 커진다 —
	//	상한을 걸어 **세우는 쪽으로만** 깎는다(자세한 이유는 MaxCardTiltDegrees 주석).
	Out.AngleDegrees = FMath::Clamp(AngleDegrees, -MaxCardTiltDegrees, MaxCardTiltDegrees);

	return Out;
}

int32 UBGHandWidget::CountRevealedCards() const
{
	int32 Revealed = 0;
	for (const UBGCardWidget* CardWidget : CardWidgets)
	{
		//	Pending 은 stagger 대기 중이라 화면에 없다(BeginDeal 이 Hidden 으로 숨긴다).
		//	없는 카드 몫까지 자리를 비워 두면 부채가 미리 다 열려 버린다.
		if (CardWidget && CardWidget->GetMotionState() != EBGCardMotionState::Pending)
		{
			++Revealed;
		}
	}

	return Revealed;
}

float UBGHandWidget::ComputeHoverSpread(int32 Index, int32 HoveredIndex) const
{
	if (HoveredIndex == INDEX_NONE || Index == HoveredIndex || HoverSpreadAmount <= 0.f)
	{
		return 0.f;
	}

	const int32 Distance = FMath::Abs(Index - HoveredIndex);
	if (Distance > HoverSpreadRange)
	{
		return 0.f;
	}

	//	한 칸 멀어질 때마다 falloff 를 한 번 더 곱한다. 전부 같은 거리로 밀면
	//	손패가 통째로 평행이동해 보여서 "한 장이 비집고 나온다" 로 안 읽힌다.
	const float Falloff = FMath::Pow(HoverSpreadFalloff, static_cast<float>(Distance - 1));

	//	왼쪽 이웃은 왼쪽으로, 오른쪽 이웃은 오른쪽으로.
	return (Index < HoveredIndex ? -1.f : 1.f) * HoverSpreadAmount * Falloff;
}

void UBGHandWidget::RelayoutCards(bool bSnap)
{
	if (!GetCardCanvas())
	{
		return;   // 구형 패널 — 배치는 패널이 알아서 한다.
	}

	const int32 Count = CardWidgets.Num();

	//	자리를 계산할 때 세는 장수. 아직 안 나타난 카드를 빼면 손패가 한 장씩 벌어진다.
	const int32 LayoutCount = bProgressiveFanOpen
		? FMath::Max(CountRevealedCards(), 1)
		: Count;

	//	마우스가 올라간 카드의 **인덱스**. Z 순서에 쓰는 InstanceId 와 달리
	//	이웃을 밀려면 손패에서 몇 번째인지가 필요하다.
	int32 HoveredIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const UBGCardWidget* CardWidget = CardWidgets[Index];
		if (CardWidget && CardWidget->IsHoverActive() && !CardWidget->IsLeavingHand())
		{
			HoveredIndex = Index;
			break;
		}
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		UBGCardWidget* CardWidget = CardWidgets[Index];
		if (!CardWidget)
		{
			continue;
		}

		//	아직 안 나타난 카드는 제 인덱스에 해당하는 자리가 없다. 그때는 맨 끝자리를
		//	목표로 준다 — 어차피 숨어 있고, 나타나는 순간 LayoutCount 가 늘면서
		//	제자리로 밀려간다. 목표를 안 주면 비행 첫 프레임에 엉뚱한 곳을 향한다.
		const int32 SlotIndex = FMath::Min(Index, LayoutCount - 1);
		FBGCardSlotLayout SlotLayout = ComputeCardSlot(SlotIndex, LayoutCount);
		SlotLayout.Position.X += ComputeHoverSpread(Index, HoveredIndex);

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
				CardWidget->IsDealInProgress() ? BGHand::DealingCardZOrder :
				CardWidget->IsHoverActive()    ? BGHand::HoveredCardZOrder :
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

void UBGHandWidget::SetTargetingAnchor(const FVector2D& InLocalPosition)
{
	TargetingAnchor = InLocalPosition;
}

void UBGHandWidget::SetPlayZoneWidget(UBGPlayZoneWidget* InPlayZone)
{
	PlayZoneWidget = InPlayZone;
}

UBGPlayZoneWidget* UBGHandWidget::ResolvePlayZoneWidget() const
{
	if (UBGPlayZoneWidget* Cached = PlayZoneWidget.Get())
	{
		return Cached;
	}

	//	HUD 계층을 모른 채 존을 찾는 유일한 방법이다 — 손패가 HUD 구조를 알게 되면
	//	그 HUD 밖에서는 못 쓰게 된다(앵커를 주입받는 것과 같은 이유다).
	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		const_cast<UBGHandWidget*>(this), Found,
		UBGPlayZoneWidget::StaticClass(), /*TopLevelOnly=*/false);

	for (UUserWidget* Widget : Found)
	{
		if (UBGPlayZoneWidget* Zone = Cast<UBGPlayZoneWidget>(Widget))
		{
			PlayZoneWidget = Zone;
			return Zone;
		}
	}

	return nullptr;
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
	//	명시로 박아 준 자리가 있으면 그게 최우선이다(BP 가 직접 잡아 준 경우).
	if (PlayZoneAnchor.IsSet())
	{
		return *PlayZoneAnchor;
	}

	//	플레이 존의 중앙을 매 호출마다 읽는다. 예전에는 드롭한 사람만 이 좌표를 알았고
	//	남이 낸 카드는 폴백 자리에 섰다 — 같은 판인데 화면마다 카드가 다른 곳에 있었다.
	if (const UCanvasPanel* Canvas = GetCardCanvas())
	{
		if (const UBGPlayZoneWidget* Zone = ResolvePlayZoneWidget())
		{
			const FGeometry& ZoneGeometry   = Zone->GetCachedGeometry();
			const FGeometry& CanvasGeometry = Canvas->GetCachedGeometry();
			const FVector2D  ZoneSize       = ZoneGeometry.GetLocalSize();

			//	첫 프레임에는 지오메트리가 0 이다. 그대로 쓰면 카드가 좌상단에 선다.
			if (!ZoneSize.IsNearlyZero() && !CanvasGeometry.GetLocalSize().IsNearlyZero())
			{
				return CanvasGeometry.AbsoluteToLocal(
					ZoneGeometry.LocalToAbsolute(ZoneSize * 0.5f));
			}
		}
	}

	return FVector2D(
		GetLayoutWidth() * 0.5f + PlayZoneAnchorFallback.X,
		GetLayoutHeight() + PlayZoneAnchorFallback.Y);
}

FVector2D UBGHandWidget::GetTargetingAnchor() const
{
	//	못 박아 준 자리가 있으면 그게 최우선이다(SetPlayZoneAnchor 와 같은 순서).
	if (TargetingAnchor.IsSet())
	{
		return *TargetingAnchor;
	}

	//	기본은 플레이 존 중앙에서 오프셋만큼 비킨 자리다. 절대 좌표로 두지 않는 이유는
	//	GetPlayZoneAnchor 와 같다 — 존이 움직이거나 해상도가 바뀌면 이 카드만 남는다.
	return GetPlayZoneAnchor() + TargetingAnchorOffset;
}

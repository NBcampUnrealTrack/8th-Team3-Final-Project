// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widget/BGCardWidget.h"
#include "UI/Widget/BGCardDragDropOperation.h"
#include "UI/Widget/BGHandWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace
{
	/**
	 * 목표를 살짝 지나쳤다 되돌아오는 감속(ease-out-back).
	 *
	 * 등장에 "탄력" 을 주는 고전적인 곡선이다. Overshoot 이 0 이면 평범한 감속이 된다.
	 */
	float EaseOutBack(float Alpha, float Overshoot)
	{
		const float A = Alpha - 1.f;
		return 1.f + (Overshoot + 1.f) * A * A * A + Overshoot * A * A;
	}

	/**
	 * 시작→목표 직선의 수직 방향. 궤적을 활처럼 휘게 하는 데 쓴다.
	 * 항상 "위로 볼록" 하도록 부호를 맞춘다 — 아래로 처지면 카드가 떨어지는 것처럼 보인다.
	 */
	FVector2D UpwardPerpendicular(const FVector2D& Delta)
	{
		FVector2D Perpendicular(-Delta.Y, Delta.X);
		if (Perpendicular.Y > 0.f)
		{
			Perpendicular = -Perpendicular;   // 화면 좌표는 아래로 갈수록 Y 가 크다
		}

		//	출발점과 목표가 거의 같으면 방향을 정할 수 없다. 그냥 위로 둔다.
		return Perpendicular.Normalize() ? Perpendicular : FVector2D(0.f, -1.f);
	}
}

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

// ══════════════════════════════════════════════════════════════════════════════════════
//  연출 (Presentation)
//
//  게임 상태와 화면 상태는 시간축이 다르다 — 손패에서 이미 빠진 카드도 Exiting 동안
//  화면에 남는다. 여기 있는 값은 전부 클라 로컬이고 서버는 알지 못한다.
// ══════════════════════════════════════════════════════════════════════════════════════

bool UBGCardWidget::IsLeavingHand() const
{
	return MotionState == EBGCardMotionState::Presenting
		|| MotionState == EBGCardMotionState::Dwelling
		|| MotionState == EBGCardMotionState::Exiting;
}

void UBGCardWidget::SetLayoutTarget(const FVector2D& InPosition, float InAngleDegrees)
{
	//	떠나는 중인 카드는 손패 자리로 되돌리면 안 된다 — 이미 손패 소속이 아니다.
	if (IsLeavingHand())
	{
		return;
	}

	TargetPosition = InPosition;
	TargetAngle    = InAngleDegrees;

	//	Pending 은 그대로 둔다. 대기가 끝나면 Moving 으로 넘어가면서 이 목표를 향한다.
	if (MotionState == EBGCardMotionState::Idle)
	{
		MotionState = EBGCardMotionState::Moving;
	}
}

void UBGCardWidget::SnapToLayoutTarget()
{
	//	⚠️ 등장/퇴장 중에는 스냅하지 않는다.
	//	   손패는 패널 크기를 처음 알게 된 프레임에 스냅을 건다. 그런데 판 첫 손패는
	//	   바로 그 프레임에 배분되므로, 여기서 걸러내지 않으면 "게임 첫 드로우만
	//	   연출 없이 툭 나타나는" 상태가 된다.
	if (bDealInProgress || IsLeavingHand())
	{
		return;
	}

	CurrentPosition = TargetPosition;
	CurrentAngle    = TargetAngle;
	CurrentScale    = 1.f;
	MotionState     = EBGCardMotionState::Idle;

	ApplyMotionTransform();
}

void UBGCardWidget::SetMotionPosition(const FVector2D& InPosition)
{
	CurrentPosition = InPosition;
	ApplyMotionTransform();
}

void UBGCardWidget::BeginDeal(const FVector2D& FromPosition, float Delay, const FBGCardDealContext& Context)
{
	CurrentPosition   = FromPosition;
	DealStartPosition = FromPosition;
	CurrentAngle      = TargetAngle + DealSpinDegrees;   // 기울어진 채로 출발해 날아오며 펴진다
	CurrentScale      = DealStartScale;
	DealElapsed       = 0.f;

	DealDelayRemaining = FMath::Max(0.f, Delay);
	DealContext        = Context;
	DealContext.FromPosition = FromPosition;   // 손패가 안 채웠어도 여기서는 확실히 안다
	bDealInProgress    = true;   // 자리에 도착할 때까지 스냅에서 보호한다

	if (DealFadeInDuration > 0.f)
	{
		SetRenderOpacity(0.f);
	}

	ApplyMotionTransform();

	if (DealDelayRemaining <= 0.f)
	{
		//	대기가 없으면 숨겼다 켜는 한 프레임 깜빡임이 생기므로 바로 출발한다.
		MotionState = (DealFlightDuration > 0.f)
			? EBGCardMotionState::Dealing
			: EBGCardMotionState::Moving;
		OnDealt(DealContext);
		return;
	}

	//	⚠️ 원래 가시성을 기억해 둔다. 되돌릴 때 Visible 로 하드코딩하면
	//	   WBP 가 정한 히트테스트 설정(SelfHitTestInvisible 등)을 덮어쓴다.
	PreDealVisibility = GetVisibility();
	SetVisibility(ESlateVisibility::Hidden);
	MotionState = EBGCardMotionState::Pending;
}

void UBGCardWidget::BeginExit(const FVector2D& ToPosition)
{
	if (IsLeavingHand())
	{
		return;   // 이미 나가는 중이다. 다시 부르면 연출이 처음부터 되감긴다.
	}

	//	대기 중에 빠진 카드는 나타난 적이 없다. 숨은 채로 퇴장시키면
	//	"보이지도 않았는데 날아가는" 그림이 되므로 가시성을 되돌려 둔다.
	if (MotionState == EBGCardMotionState::Pending)
	{
		SetVisibility(PreDealVisibility);
	}

	//	퇴장 중에는 입력을 받지 않는다 — 날아가는 카드를 다시 집을 수 있으면 안 된다.
	PreLeaveVisibility = GetVisibility();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	bDealInProgress  = false;
	bHoldingAtCenter = false;   // 곧장 퇴장은 잡아 두지 않는다

	EnterExitPhase(ToPosition);
}

void UBGCardWidget::BeginPlayExit(const FVector2D& CenterPosition, const FVector2D& ToPosition)
{
	if (IsLeavingHand())
	{
		return;
	}

	if (MotionState == EBGCardMotionState::Pending)
	{
		SetVisibility(PreDealVisibility);
	}

	PreLeaveVisibility = GetVisibility();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	bDealInProgress = false;

	//	중앙에 도착하면 놓아 줄 때까지 기다린다. 대상을 고르는 동안 카드가
	//	중앙에 서 있어야 무엇을 내고 있는지 보인다.
	bHoldingAtCenter = true;

	PresentStartPosition  = CurrentPosition;
	PresentStartScale     = CurrentScale;
	PresentStartAngle     = CurrentAngle;
	PresentTargetPosition = CenterPosition;
	PresentElapsed        = 0.f;

	//	대기가 끝난 뒤에 쓸 목적지. 지금 잡아 두는 이유는 그 시점에 손패가
	//	이미 다른 상태일 수 있어서다(앵커가 바뀌었을 수도 있다).
	PendingExitPosition = ToPosition;

	MotionState = EBGCardMotionState::Presenting;
}

FBGCardPlayContext UBGCardWidget::ReleaseHold(const FBGCardPlayContext& Context)
{
	//	부르는 쪽은 판정만 알고 카드 데이터는 모른다. 빈 칸은 여기서 메운다 —
	//	두 해제 경로(판정 통지 / 손패 갱신 폴백)가 같은 결과를 갖게 하는 지점이다.
	FBGCardPlayContext Filled = Context;
	if (Filled.Archetype == EBGCardFXArchetype::None)
	{
		Filled.Archetype = CardView.FXArchetype;
	}

	if (!bHoldingAtCenter)
	{
		return Filled;   // 이미 풀렸다. 맥락만 돌려준다.
	}

	bHoldingAtCenter = false;

	//	대기 시간은 "풀린 시점" 부터 센다. 중앙에 도착한 시점부터 세면
	//	대상을 오래 고른 사람은 효과를 볼 시간이 없다.
	DwellRemaining = DwellDuration;

	//	아직 중앙으로 가는 중이면 도착 후 자연스럽게 대기로 넘어간다.
	if (MotionState == EBGCardMotionState::Dwelling || MotionState == EBGCardMotionState::Presenting)
	{
		OnPlayResolved(Filled);
	}

	return Filled;
}

bool UBGCardWidget::ReturnToHand()
{
	//	이미 사라지는 중이면 되돌리지 않는다. 반투명해진 카드가 손패에 다시
	//	나타나면 더 이상해 보인다 — 그냥 사라지게 두고 손패가 새로 만든다.
	if (!IsLeavingHand() || MotionState == EBGCardMotionState::Exiting)
	{
		return false;
	}

	bHoldingAtCenter = false;
	DwellRemaining   = 0.f;

	SetRenderOpacity(1.f);
	SetVisibility(PreLeaveVisibility);

	//	손패가 곧 SetLayoutTarget 으로 자리를 알려 준다. 그때까지는 현재 위치에서 기다린다.
	MotionState = EBGCardMotionState::Moving;

	OnReturnedToHand();
	return true;
}

void UBGCardWidget::EnterExitPhase(const FVector2D& ToPosition)
{
	ExitStartPosition = CurrentPosition;
	ExitStartScale    = CurrentScale;
	ExitElapsed       = 0.f;
	TargetPosition    = ToPosition;
	MotionState       = EBGCardMotionState::Exiting;

	OnExitStarted();
}

bool UBGCardWidget::IsExitFinished() const
{
	//	Presenting / Dwelling 은 아직 끝이 아니다 — 여기서 true 를 돌려주면
	//	중앙에 올라가자마자 파괴된다.
	return MotionState == EBGCardMotionState::Exiting && ExitElapsed >= ExitDuration;
}

bool UBGCardWidget::IsHoverActive() const
{
	//	⚠️ 보간값(HoverBlend)이 아니라 실제 마우스 상태를 본다.
	//	   연출이 풀리는 동안에도 위층에 남아 있으면, 커서를 옆 카드로 옮긴 직후
	//	   방금 떠난 카드가 새로 고른 카드를 잠깐 가린다.
	return bHovered;
}

void UBGCardWidget::TickMotion(float DeltaTime)
{
	const FVector2D PositionBeforeTick = CurrentPosition;

	switch (MotionState)
	{
	case EBGCardMotionState::Pending:
	{
		DealDelayRemaining -= DeltaTime;
		if (DealDelayRemaining > 0.f)
		{
			return;   // 아직 숨어 있다. 트랜스폼도 건드릴 것이 없다.
		}

		SetVisibility(PreDealVisibility);
		MotionState = (DealFlightDuration > 0.f)
			? EBGCardMotionState::Dealing
			: EBGCardMotionState::Moving;
		OnDealt(DealContext);
		break;
	}

	case EBGCardMotionState::Dealing:
	{
		DealElapsed += DeltaTime;

		const float Alpha = FMath::Clamp(DealElapsed / FMath::Max(DealFlightDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

		//	목표는 매 프레임 다시 읽는다 — 뒤이어 다른 카드가 들어오면 자리가 밀린다.
		//	시작점만 고정이므로 궤적이 목표를 따라 자연스럽게 휜다.
		const FVector2D Delta  = TargetPosition - DealStartPosition;
		const float PositionEased = FMath::InterpEaseOut(0.f, 1.f, Alpha, DealEaseExponent);

		//	활 모양 궤적. sin 은 양끝이 0 이라 출발점·도착점은 정확히 유지된다.
		const float ArcOffset = DealArcHeight * FMath::Sin(PI * Alpha);

		CurrentPosition = FMath::Lerp(DealStartPosition, TargetPosition, PositionEased)
			+ UpwardPerpendicular(Delta) * ArcOffset;

		//	기울기를 편다. 위치보다 조금 빨리 정렬돼야 도착 순간에 반듯해 보인다.
		CurrentAngle = FMath::Lerp(TargetAngle + DealSpinDegrees, TargetAngle, PositionEased);

		//	제 크기를 살짝 지나쳤다 돌아온다 — "툭 놓이는" 탄력.
		CurrentScale = FMath::Lerp(DealStartScale, 1.f, EaseOutBack(Alpha, DealScaleOvershoot));

		if (DealFadeInDuration > 0.f)
		{
			SetRenderOpacity(FMath::Clamp(DealElapsed / DealFadeInDuration, 0.f, 1.f));
		}

		if (Alpha >= 1.f)
		{
			//	투명도를 확실히 되돌린다. 반올림으로 0.99 가 남으면 손패 카드 하나만
			//	미묘하게 흐린 채로 굳는다.
			SetRenderOpacity(1.f);

			//	보간 단계로 넘긴다. 여기서 Idle 로 바로 가지 않는 이유는 비행 중에
			//	목표가 바뀌었을 수 있어서다 — 남은 오차를 보간이 부드럽게 흡수한다.
			MotionState = EBGCardMotionState::Moving;
		}
		break;
	}

	case EBGCardMotionState::Moving:
	{
		CurrentPosition = FMath::Vector2DInterpTo(CurrentPosition, TargetPosition, DeltaTime, MoveInterpSpeed);
		CurrentAngle    = FMath::FInterpTo(CurrentAngle, TargetAngle, DeltaTime, AngleInterpSpeed);
		CurrentScale    = FMath::FInterpTo(CurrentScale, 1.f, DeltaTime, ScaleInterpSpeed);

		//	InterpTo 는 목표에 정확히 닿지 않는다. 충분히 가까우면 붙이고 틱을 멈춘다 —
		//	안 그러면 손패의 모든 카드가 영원히 미세하게 움직이며 틱을 먹는다.
		const bool bArrived =
			CurrentPosition.Equals(TargetPosition, 0.5f)
			&& FMath::IsNearlyEqual(CurrentAngle, TargetAngle, 0.1f)
			&& FMath::IsNearlyEqual(CurrentScale, 1.f, 0.005f);

		if (bArrived)
		{
			CurrentPosition = TargetPosition;
			CurrentAngle    = TargetAngle;
			CurrentScale    = 1.f;
			MotionState     = EBGCardMotionState::Idle;
			bDealInProgress = false;
		}
		break;
	}

	case EBGCardMotionState::Presenting:
	{
		PresentElapsed += DeltaTime;

		const float Alpha = FMath::Clamp(PresentElapsed / FMath::Max(PresentDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		//	중앙으로는 빠르게 나갔다가 부드럽게 멈춘다 — "탁 놓는" 느낌.
		const float Eased = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);

		CurrentPosition = FMath::Lerp(PresentStartPosition, PresentTargetPosition, Eased);
		CurrentScale    = FMath::Lerp(PresentStartScale, PresentScale, Eased);
		//	부채꼴로 기울어 있던 각도를 편다. 중앙에서는 똑바로 서야 읽기 좋다.
		CurrentAngle    = FMath::Lerp(PresentStartAngle, 0.f, Eased);

		if (Alpha >= 1.f)
		{
			MotionState = EBGCardMotionState::Dwelling;
			OnPlayPresented();
		}
		break;
	}

	case EBGCardMotionState::Dwelling:
	{
		//	중앙에 가만히 서 있는다.
		//	  잡혀 있는 동안(대상 선택 대기)   → 시간이 흐르지 않는다
		//	  풀린 뒤(효과 발동됨)            → DwellDuration 만큼 더 보여 주고 사라진다
		if (bHoldingAtCenter)
		{
			break;   // 부수 효과는 계속 굴러야 기울기가 0 으로 되돌아온다
		}

		DwellRemaining -= DeltaTime;
		if (DwellRemaining <= 0.f)
		{
			EnterExitPhase(PendingExitPosition);
		}
		break;
	}

	case EBGCardMotionState::Exiting:
	{
		ExitElapsed += DeltaTime;

		const float Alpha = FMath::Clamp(ExitElapsed / FMath::Max(ExitDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

		CurrentPosition = FMath::Lerp(ExitStartPosition, TargetPosition, FMath::InterpEaseIn(0.f, 1.f, Alpha, 2.f));
		CurrentScale    = FMath::Lerp(ExitStartScale, ExitEndScale, Alpha);
		SetRenderOpacity(1.f - Alpha);
		break;
	}

	default:
		//	Idle — 주 움직임은 없지만 부수 효과는 계속 굴러야 한다.
		//	기울기가 0 으로 풀리고 호버가 붙고 떨어지는 것이 전부 여기서 일어난다.
		break;
	}

	TickSecondaryMotion(DeltaTime, PositionBeforeTick);

	//	완전히 정지했고 얹힐 효과도 없으면 트랜스폼을 건드리지 않는다 —
	//	매 프레임 SetPosition 을 부르면 손패 전체가 계속 슬레이트를 무효화한다.
	const bool bFullyAtRest = MotionState == EBGCardMotionState::Idle
		&& FMath::IsNearlyZero(VelocityTilt, 0.01f)
		&& FMath::IsNearlyZero(HoverBlend, 0.001f);

	if (!bFullyAtRest)
	{
		ApplyMotionTransform();
	}
}

void UBGCardWidget::TickSecondaryMotion(float DeltaTime, const FVector2D& PositionBeforeTick)
{
	if (DeltaTime <= 0.f)
	{
		return;
	}

	// ── 관성 기울기 ──
	//	속도는 위치 차분으로 구한다. 별도 변수로 적분하면 스냅이나 순간이동
	//	(드롭 지점으로 옮기는 등) 때 실제 화면 움직임과 어긋난다.
	const float HorizontalSpeed = (CurrentPosition.X - PositionBeforeTick.X) / DeltaTime;

	//	이동 방향으로 눕는다. 부호가 반대면 진행 방향을 거슬러 서는 것처럼 보인다.
	const float DesiredTilt = FMath::Clamp(
		HorizontalSpeed * VelocityTiltPerSpeed, -MaxVelocityTilt, MaxVelocityTilt);

	VelocityTilt = FMath::FInterpTo(VelocityTilt, DesiredTilt, DeltaTime, VelocityTiltInterpSpeed);

	// ── 호버 ──
	//	손패에 얌전히 있는 카드만 반응한다. 날아오는 중이거나 떠나는 중인 카드가
	//	커서에 반응하면 연출이 두 개 겹쳐 흔들린다.
	const bool bHoverAllowed = bHovered
		&& !bIsDragVisual
		&& !bBeingDragged
		&& !bDragLocked
		&& !IsLeavingHand()
		&& !bDealInProgress;

	HoverBlend = FMath::FInterpTo(HoverBlend, bHoverAllowed ? 1.f : 0.f, DeltaTime, HoverInterpSpeed);
}

void UBGCardWidget::ApplyMotionTransform()
{
	//	배치가 정한 값 위에 부수 효과를 더한다. 순서가 아니라 합이다 —
	//	배치는 배치대로, 호버는 호버대로 독립적으로 계산돼야 서로를 지우지 않는다.
	const FVector2D HoverOffset(0.f, HoverLift * HoverBlend);
	const float HoverTilt = HoverLocalNormalized.X * HoverTiltMax * HoverBlend;
	const float FinalScale = CurrentScale * FMath::Lerp(1.f, HoverScale, HoverBlend);

	//	'Slot' 은 UWidget 의 멤버명이라 지역 변수 이름을 달리한다 (C4458 은 에러로 승격됨).
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetPosition(CurrentPosition + HoverOffset);
	}

	//	캔버스가 아니면(구형 HorizontalBox 손패) 위치는 패널이 정한다 — 회전/크기만 반영된다.
	SetRenderTransformAngle(CurrentAngle + VelocityTilt + HoverTilt);
	SetRenderScale(FVector2D(FinalScale, FinalScale));

	//	비틀림으로 입체감을 낸다. 커서가 위쪽이면 카드 윗변이 뒤로 눕는 것처럼 보인다.
	//	UMG 는 2D 라 진짜 3D 회전은 없지만, shear 로 그 인상을 만들 수 있다.
	if (HoverShearMax != 0.f)
	{
		SetRenderShear(FVector2D(
			-HoverLocalNormalized.Y * HoverShearMax * HoverBlend,
			 HoverLocalNormalized.X * HoverShearMax * HoverBlend));
	}
}

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
		HoverLocalNormalized = FVector2D(
			FMath::Clamp((Local.X / LocalSize.X - 0.5f) * 2.f, -1.f, 1.f),
			FMath::Clamp((Local.Y / LocalSize.Y - 0.5f) * 2.f, -1.f, 1.f));
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
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

// UBGCardWidget — 모션 상태 기계 (EBGCardMotionState).
//
// 상태 전이("언제 어디로 간다")와 매 프레임 보간("지금 어디쯤인가")이 한 파일에 있다.
// 둘을 가르지 않은 이유: 전이 조건이 보간 진행도(Alpha >= 1)에서 나오는 자리가 많아
// 나누면 같은 흐름을 두 파일에서 번갈아 읽어야 한다.
//
// 진입점은 TickMotion 하나다 — 손패 위젯이 매 프레임 불러 준다.
//
// ⚠️ 여기 있는 값은 전부 클라 로컬이고 서버는 알지 못한다. 게임 상태와 화면 상태는
//    시간축이 다르다 — 손패에서 이미 빠진 카드도 Exiting 동안 화면에 남는다.

#include "UI/Widget/BGCardWidget.h"
#include "UI/Widget/BGCardWidgetMotionMath.h"

#include "Components/CanvasPanelSlot.h"
#include "Curves/CurveFloat.h"
#include "Debug/BGCardLog.h"


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
	//	⚠️ 등장/퇴장 중에는 스냅하지 않는다. 손패는 패널 크기를 처음 안 프레임에 스냅을 거는데
	//	   판 첫 손패가 바로 그 프레임에 배분되므로, 안 거르면 "첫 드로우만 연출 없이 툭" 나온다.
	if (Deal.bInProgress || IsLeavingHand())
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
	CurrentPosition = FromPosition;
	CurrentAngle    = TargetAngle + DealSpinDegrees;   // 기울어진 채로 출발해 날아오며 펴진다
	CurrentScale    = DealStartScale;

	//	비행 상태를 한 번에 초기화한다 — 겨냥은 첫 비행 프레임에 다시 잡는다.
	//	지금 TargetPosition 을 읽으면 손패가 아직 자리를 다시 잡기 전이라 옛 자리를
	//	겨눈다(SetHand 는 BeginDeal 을 먼저 부른다).
	Deal.Begin(FromPosition, FMath::Max(0.f, Delay), Context);
	Deal.Context.FromPosition = FromPosition;   // 손패가 안 채웠어도 여기서는 확실히 안다

	if (DealFadeInDuration > 0.f)
	{
		SetRenderOpacity(0.f);
	}

	ApplyMotionTransform();

	if (Deal.DelayRemaining <= 0.f)
	{
		//	대기가 없으면 숨겼다 켜는 한 프레임 깜빡임이 생기므로 바로 출발한다.
		MotionState = (DealFlightDuration > 0.f)
			? EBGCardMotionState::Dealing
			: EBGCardMotionState::Moving;
		OnDealt(Deal.Context);
		return;
	}

	//	⚠️ 원래 가시성을 기억해 둔다. Visible 로 하드코딩하면 WBP 가 정한
	//	   히트테스트 설정(SelfHitTestInvisible 등)을 덮어쓴다.
	Deal.PreVisibility = GetVisibility();
	SetVisibility(ESlateVisibility::Hidden);
	MotionState = EBGCardMotionState::Pending;
}

bool UBGCardWidget::BeginLeaveHand()
{
	if (IsLeavingHand())
	{
		return false;   // 이미 나가는 중이다. 다시 부르면 연출이 처음부터 되감긴다.
	}

	//	대기 중에 빠진 카드는 나타난 적이 없다 — 숨은 채로 퇴장시키면
	//	"보이지도 않았는데 날아가는" 그림이 되므로 가시성을 되돌려 둔다.
	if (MotionState == EBGCardMotionState::Pending)
	{
		SetVisibility(Deal.PreVisibility);
	}

	//	떠나는 중에는 입력을 받지 않는다 — 날아가는 카드를 다시 집을 수 있으면 안 된다.
	PreLeaveVisibility = GetVisibility();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	Deal.bInProgress = false;

	return true;
}

void UBGCardWidget::BeginExit(const FVector2D& ToPosition)
{
	if (!BeginLeaveHand())
	{
		return;
	}

	Center.bHolding = false;   // 곧장 퇴장은 잡아 두지 않는다
	bPlayExit       = false;   // 버려지는 카드다 — 사용한 카드와 사라지는 방식이 다르다

	EnterExitPhase(ToPosition);
}

void UBGCardWidget::BeginPlayExit(const FVector2D& CenterPosition, const FVector2D& ToPosition)
{
	if (!BeginLeaveHand())
	{
		return;
	}

	bPlayExit = true;   // 사용한 카드다. 대기가 끝난 뒤 디졸브로 사라진다

	//	중앙에 도착하면 놓아 줄 때까지 기다린다(bHolding) — 대상을 고르는 동안 카드가
	//	서 있어야 무엇을 내고 있는지 보인다. 퇴장 목적지를 지금 잡아 두는 이유는
	//	그 시점에 손패가 이미 다른 상태일 수 있어서다(앵커가 바뀌었을 수도 있다).
	//	밀려 있던 앞 사용의 신호도 여기서 함께 끊긴다.
	Center.Begin(CurrentPosition, CurrentScale, CurrentAngle, CenterPosition, ToPosition);

	MotionState = EBGCardMotionState::Presenting;
}

void UBGCardWidget::FinishDwell()
{
	//	아직 잡혀 있다(사용 확정 전) — 판정 연출의 신호가 아니므로 버린다.
	//	기억해 두면 나중에 판정이 올 때 연출을 건너뛰고 곧장 사라진다.
	if (Center.bHolding)
	{
		return;
	}

	//	중앙에 닿기 전에 온 신호는 버리지 않고 도착하는 순간 소비한다 —
	//	버리면 "연출이 끝나면 디졸브" 가 애니메이션 길이에 따라 조용히 깨진다.
	if (MotionState == EBGCardMotionState::Presenting)
	{
		Center.bDwellFinishRequested = true;
		return;
	}

	//	그 밖의 상태에서는 조용히 넘어간다. Finished 는 중복으로 오고,
	//	퇴장에 들어간 카드를 다시 퇴장시키면 궤적이 처음부터 다시 그려진다.
	if (MotionState != EBGCardMotionState::Dwelling)
	{
		return;
	}

	Center.DwellRemaining = 0.f;
	EnterExitPhase(Center.PendingExitPosition);
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

	//	⚠️ Archetype 과 달리 조건 없이 덮어쓴다. 기본값(Magic)과 "안 채워짐" 을 구분할
	//	   방법이 없고, 카드 뷰가 언제나 정답이기 때문이다.
	Filled.Category = CardView.Category;

	if (!Center.bHolding)
	{
		return Filled;   // 이미 풀렸다. 맥락만 돌려준다.
	}

	Center.bHolding = false;

	//	대기 시간은 "풀린 시점" 부터 센다. 도착 시점부터 세면 대상을 오래 고른 사람은
	//	효과를 볼 시간이 없다.
	Center.DwellRemaining = DwellDuration;

	//	아직 중앙으로 가는 중이면 연출은 도착한 뒤에 터뜨린다 —
	//	날아가는 동안 재생하면 애니메이션이 중앙에 서기도 전에 끝나 버린다.
	if (MotionState == EBGCardMotionState::Presenting)
	{
		Center.bPendingResolvedFX     = true;
		Center.PendingResolvedContext = Filled;
		return Filled;
	}

	if (MotionState == EBGCardMotionState::Dwelling)
	{
		PlayResolvedFX(Filled);
	}

	return Filled;
}

bool UBGCardWidget::ReturnToHand()
{
	//	이미 사라지는 중이면 되돌리지 않는다 — 반투명해진 카드가 손패에 다시 나타나면
	//	더 이상해 보인다. 그냥 사라지게 두고 손패가 새로 만든다.
	if (!IsLeavingHand() || MotionState == EBGCardMotionState::Exiting)
	{
		return false;
	}

	Center.bHolding       = false;
	Center.DwellRemaining = 0.f;
	bPlayExit             = false;   // 더 이상 사용 중인 카드가 아니다

	//	손패로 돌아간 카드에 판정 연출이 뒤늦게 터지면 안 된다.
	Center.ClearPendingSignals();

	SetRenderOpacity(1.f);
	SetVisibility(PreLeaveVisibility);

	//	손패가 곧 SetLayoutTarget 으로 자리를 알려 준다. 그때까지는 현재 위치에서 기다린다.
	MotionState = EBGCardMotionState::Moving;

	OnReturnedToHand();
	return true;
}

void UBGCardWidget::EnterExitPhase(const FVector2D& ToPosition)
{
	//	밀린 신호는 전부 버린다 — 타기 시작한 카드 위의 연출은 디졸브에 묻힌다.
	Center.ClearPendingSignals();

	ExitStartPosition = CurrentPosition;
	ExitStartScale    = CurrentScale;
	ExitElapsed       = 0.f;
	TargetPosition    = ToPosition;
	MotionState       = EBGCardMotionState::Exiting;

	//	⚠️ BP 훅보다 먼저 건다. BP 가 무엇을 얹든 카드는 이미 타기 시작해야
	//	   "사라지는 중" 으로 읽힌다(PlayTierFX 를 OnPlayResolved 앞에 두는 것과 같은 이유).
	BeginDissolveExit();

	OnExitStarted();

	//	카드가 자리를 뜨면 화면 연출도 같이 내린다.
	//	⚠️ 바깥이 끝을 정하기로 했으면 여기서 끄지 않는다 — 몽타주가 아직 돌고 있다.
	if (!bWaitForExternalPlayFXFinish)
	{
		NotifyPlayFXFinished();
	}
}

void UBGCardWidget::NotifyPlayFXFinished()
{
	//	⚠️ 켠 적 없음과 중복 호출을 여기서 함께 거른다. 몽타주 종료는 중단·블렌드아웃으로
	//	   두 번 오고, 퇴장과 파괴 두 경로가 모두 이 함수를 지난다.
	if (!bPlayFXStarted)
	{
		return;
	}

	bPlayFXStarted = false;
}

void UBGCardWidget::MoveHoldTo(const FVector2D& NewCenterPosition)
{
	//	중앙에 잡혀 있는 동안만 의미가 있다. 이미 풀렸거나 퇴장 중이면 궤적만 흐트러진다.
	if (!Center.bHolding)
	{
		return;
	}

	//	이미 그 자리를 향하고 있다. 다시 걸면 도착해 있던 카드가 PresentDuration 만큼
	//	더 서 있게 되고 판정 연출도 그만큼 늦어진다.
	if (Center.TargetPosition.Equals(NewCenterPosition, 1.f))
	{
		return;
	}

	Center.StartPosition  = CurrentPosition;
	Center.StartScale     = CurrentScale;
	Center.StartAngle     = CurrentAngle;
	Center.TargetPosition = NewCenterPosition;
	Center.Elapsed        = 0.f;

	//	다시 날아가는 중이 된다 — 판정 연출은 도착한 뒤에 터진다(ReleaseHold 와 같은 규칙).
	MotionState = EBGCardMotionState::Presenting;
}

bool UBGCardWidget::IsExitFinished() const
{
	//	Presenting / Dwelling 은 아직 끝이 아니다 — 여기서 true 면 중앙에 올라가자마자 파괴된다.
	return MotionState == EBGCardMotionState::Exiting && ExitElapsed >= ExitDuration;
}

bool UBGCardWidget::IsHoverActive() const
{
	//	⚠️ 보간값(HoverBlend)이 아니라 실제 마우스 상태를 본다 — 연출이 풀리는 동안에도
	//	   위층에 남아 있으면 방금 떠난 카드가 새로 고른 카드를 잠깐 가린다.
	//
	//	⚠️ 억제 상태(잠금·드래그·등장/퇴장)에서는 false 여야 한다. bHovered 만 보면 손패가
	//	   잠긴 동안 카드가 **떠오르지 않으면서 Z 순서만** 올라가는데, 부채꼴에서 앞뒤가
	//	   바뀌면 커서 아래 카드가 뒤바뀌고 그게 다시 Z 순서를 바꿔 손패 전체가 깜빡인다.
	return bHovered
		&& !bIsDragVisual
		&& !bBeingDragged
		&& !bDragLocked
		&& !IsLeavingHand()
		&& !Deal.bInProgress;
}

void UBGCardWidget::TickMotion(float DeltaTime)
{
	//	등급 연출은 이동 상태와 무관하게 굴러야 한다 — Idle 로 멈춰도 잦아드는 중일 수 있다.
	TickTierFX(DeltaTime);

	const FVector2D PositionBeforeTick = CurrentPosition;

	switch (MotionState)
	{
	case EBGCardMotionState::Pending:
		if (!TickPending(DeltaTime))
		{
			return;   // 아직 숨어 있다. 트랜스폼도 건드릴 것이 없다.
		}
		break;

	case EBGCardMotionState::Dealing:    TickDealing(DeltaTime);    break;
	case EBGCardMotionState::Moving:     TickMoving(DeltaTime);     break;
	case EBGCardMotionState::Presenting: TickPresenting(DeltaTime); break;
	case EBGCardMotionState::Dwelling:   TickDwelling(DeltaTime);   break;
	case EBGCardMotionState::Exiting:    TickExiting(DeltaTime);    break;

	default:
		//	Idle — 주 움직임은 없지만 부수 효과(기울기 풀림·호버)는 계속 굴러야 한다.
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

bool UBGCardWidget::TickPending(float DeltaTime)
{
	Deal.DelayRemaining -= DeltaTime;
	if (Deal.DelayRemaining > 0.f)
	{
		return false;
	}

	SetVisibility(Deal.PreVisibility);
	MotionState = (DealFlightDuration > 0.f)
		? EBGCardMotionState::Dealing
		: EBGCardMotionState::Moving;
	OnDealt(Deal.Context);

	return true;
}

void UBGCardWidget::TickDealing(float DeltaTime)
{
	Deal.Elapsed += DeltaTime;

	const float Alpha = BGCardMotion::Progress(Deal.Elapsed, DealFlightDuration);

	if (!Deal.bAimInitialized)
	{
		//	첫 비행 프레임. 여기서야 목표가 유효하다(BeginDeal 은 손패가 자리를 다시 잡기
		//	전에 불린다). 호가 휠 방향도 이때 한 번만 정하고 고정한다.
		Deal.AimPosition      = TargetPosition;
		Deal.ArcDirection     = BGCardMotion::UpwardPerpendicular(TargetPosition - Deal.StartPosition);
		Deal.bAimInitialized  = true;
	}
	else
	{
		//	손패가 벌어지며 자리가 밀린다. 그 순간이동을 그대로 따라가지 않고
		//	겨냥이 천천히 쫓아가게 해서 궤적을 하나의 곡선으로 유지한다.
		Deal.AimPosition = FMath::Vector2DInterpTo(
			Deal.AimPosition, TargetPosition, DeltaTime, DealAimInterpSpeed);
	}

	//	가속 → 감속. 감속만 하면 첫 몇 프레임에 거리의 절반 가까이를 가버려 순간이동처럼 보인다.
	const float PositionEased = bDealEaseInOut
		? FMath::InterpEaseInOut(0.f, 1.f, Alpha, DealEaseExponent)
		: FMath::InterpEaseOut(0.f, 1.f, Alpha, DealEaseExponent);

	//	활 모양 궤적. sin 은 양끝이 0 이라 출발점·도착점은 정확히 유지된다.
	const float ArcOffset = DealArcHeight * FMath::Sin(PI * Alpha);

	CurrentPosition = FMath::Lerp(Deal.StartPosition, Deal.AimPosition, PositionEased)
		+ Deal.ArcDirection * ArcOffset;

	//	기울기를 편다. 위치보다 조금 빨리 정렬돼야 도착 순간에 반듯해 보인다.
	CurrentAngle = FMath::Lerp(TargetAngle + DealSpinDegrees, TargetAngle, PositionEased);

	//	제 크기를 살짝 지나쳤다 돌아온다 — "툭 놓이는" 탄력.
	CurrentScale = FMath::Lerp(DealStartScale, 1.f, BGCardMotion::EaseOutBack(Alpha, DealScaleOvershoot));

	if (DealFadeInDuration > 0.f)
	{
		SetRenderOpacity(BGCardMotion::Progress(Deal.Elapsed, DealFadeInDuration));
	}

	if (Alpha >= 1.f)
	{
		//	투명도를 확실히 되돌린다. 0.99 가 남으면 카드 하나만 미묘하게 흐린 채로 굳는다.
		SetRenderOpacity(1.f);

		//	보간 단계로 넘긴다. Idle 로 바로 가지 않는 이유는 비행 중에 목표가 바뀌었을 수
		//	있어서다 — 남은 오차를 보간이 부드럽게 흡수한다.
		MotionState = EBGCardMotionState::Moving;
	}
}

void UBGCardWidget::TickMoving(float DeltaTime)
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
		Deal.bInProgress = false;
	}
}

void UBGCardWidget::TickPresenting(float DeltaTime)
{
	Center.Elapsed += DeltaTime;

	const float Alpha = BGCardMotion::Progress(Center.Elapsed, PresentDuration);
	//	중앙으로는 빠르게 나갔다가 부드럽게 멈춘다 — "탁 놓는" 느낌.
	const float Eased = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);

	CurrentPosition = FMath::Lerp(Center.StartPosition, Center.TargetPosition, Eased);
	CurrentScale    = FMath::Lerp(Center.StartScale, PresentScale, Eased);
	//	부채꼴로 기울어 있던 각도를 편다. 중앙에서는 똑바로 서야 읽기 좋다.
	CurrentAngle    = FMath::Lerp(Center.StartAngle, 0.f, Eased);

	if (Alpha >= 1.f)
	{
		MotionState = EBGCardMotionState::Dwelling;
		OnPlayPresented();

		//	날아오는 사이에 도착해 있던 신호를 여기서 소비한다. 대기 시계가 도는 상태라
		//	연출 애니메이션의 Finished → FinishDwell 이 제대로 받아진다.
		ConsumePendingPlaySignals();
	}
}

void UBGCardWidget::ConsumePendingPlaySignals()
{
	//	⚠️ 판정 연출이 먼저다 — 새 연출이 시작되면 밀려 있던 FinishDwell 은 무효가 된다
	//	   (PlayResolvedFX 가 지운다). 순서가 뒤집히면 연출이 첫 프레임에 잘린다.
	if (Center.bPendingResolvedFX)
	{
		Center.bPendingResolvedFX = false;
		PlayResolvedFX(Center.PendingResolvedContext);
	}

	if (Center.bDwellFinishRequested)
	{
		Center.bDwellFinishRequested = false;
		FinishDwell();
	}
}

void UBGCardWidget::PlayResolvedFX(const FBGCardPlayContext& Context)
{
	//	새 연출이 시작된다 — 밀려 있던 종료 신호는 이 연출의 것이 아니므로 버린다.
	Center.bDwellFinishRequested = false;

	//	등급 연출을 BP 훅보다 먼저 터뜨린다. BP 가 무엇을 얹든 글로우는 이미 올라와 있어야
	//	"판정이 확정된 순간" 으로 읽힌다.
	PlayTierFX(Context.Tier);

	OnPlayResolved(Context);

	//	BP 훅 뒤에 둔다 — 카드 자신의 연출이 먼저 자리를 잡아야 화면 연출이 거기 얹힌다.
	bPlayFXStarted = true;
	OnPlayFXStarted.Broadcast(this, Context);
}

void UBGCardWidget::TickDwelling(float DeltaTime)
{
	//	중앙에 가만히 서 있는다.
	//	  잡혀 있는 동안(대상 선택 대기)   → 시간이 흐르지 않는다
	//	  풀린 뒤(효과 발동됨)            → DwellDuration 만큼 더 보여 주고 사라진다
	if (Center.bHolding)
	{
		return;   // 부수 효과는 계속 굴러야 기울기가 0 으로 되돌아온다
	}

	//	애니메이션이 길이를 쥐는 모드에서도 시계는 돈다 — 그때 DwellDuration 은 연출 길이가
	//	아니라 **안전 상한**이다. BP 가 FinishDwell 을 빠뜨리면 카드가 중앙에 영원히 박히고
	//	손패가 잠기는데, 그 상태는 에러조차 안 난다.
	Center.DwellRemaining -= DeltaTime;

	if (Center.DwellRemaining <= 0.f)
	{
		if (bWaitForDwellAnimation)
		{
			//	상한까지 왔는데 아직 신호가 없다. 연출이 잘린 것이므로 원인을 남긴다.
			UE_LOG(LogBGCard, Warning,
				TEXT("[Card] 대기 상한(%.2f초) 초과 — FinishDwell 이 안 왔다. "
				     "WBP 애니메이션 Finished 에 FinishDwell 을 물렸는지, "
				     "애니메이션이 DwellDuration 보다 길지 않은지 확인할 것."),
				DwellDuration);
		}

		EnterExitPhase(Center.PendingExitPosition);
	}
}

void UBGCardWidget::TickExiting(float DeltaTime)
{
	ExitElapsed += DeltaTime;

	const float Alpha = BGCardMotion::Progress(ExitElapsed, ExitDuration);

	CurrentPosition = FMath::Lerp(ExitStartPosition, TargetPosition, FMath::InterpEaseIn(0.f, 1.f, Alpha, 2.f));
	CurrentScale    = FMath::Lerp(ExitStartScale, ExitEndScale, Alpha);

	if (bDissolveActive)
	{
		//	⚠️ 디졸브 중에는 불투명도를 건드리지 않는다 — 타면서 동시에 흐려지면 경계의
		//	   불꽃이 묻혀 그냥 페이드아웃처럼 보인다. 투명도는 머테리얼 안에서 낼 것.
		//	원형이 자기 곡선을 들고 있으면 그것이 이긴다(공격은 뒤가 가파르고 회복은 고르게).
		const UCurveFloat* ExitCurve = ActiveDissolve.Curve ? ActiveDissolve.Curve.Get() : DissolveCurve.Get();
		ApplyDissolve(BGCardMotion::CurveOr(ExitCurve, Alpha, Alpha));
	}
	else
	{
		SetRenderOpacity(1.f - Alpha);
	}
}

void UBGCardWidget::TickSecondaryMotion(float DeltaTime, const FVector2D& PositionBeforeTick)
{
	if (DeltaTime <= 0.f)
	{
		return;
	}

	// ── 관성 기울기 ──
	//	속도는 위치 차분으로 구한다. 별도 변수로 적분하면 스냅·순간이동 때 화면과 어긋난다.
	const float HorizontalSpeed = (CurrentPosition.X - PositionBeforeTick.X) / DeltaTime;

	//	이동 방향으로 눕는다. 부호가 반대면 진행 방향을 거슬러 서는 것처럼 보인다.
	const float DesiredTilt = FMath::Clamp(
		HorizontalSpeed * VelocityTiltPerSpeed, -MaxVelocityTilt, MaxVelocityTilt);

	VelocityTilt = FMath::FInterpTo(VelocityTilt, DesiredTilt, DeltaTime, VelocityTiltInterpSpeed);

	// ── 호버 ──
	//	손패에 얌전히 있는 카드만 반응한다 — 날아오거나 떠나는 카드가 커서에 반응하면
	//	연출이 두 개 겹쳐 흔들린다.
	//	⚠️ 조건은 IsHoverActive 하나만 쓴다. 연출과 Z 순서가 각자 판단하면
	//	   "안 떠오르는데 앞으로 나오는" 상태가 생긴다.
	const bool bHoverAllowed = IsHoverActive();
	HoverBlend = FMath::FInterpTo(HoverBlend, bHoverAllowed ? 1.f : 0.f, DeltaTime, HoverInterpSpeed);

	//	커서 위치도 보간해서 쓴다 — 마우스 이벤트 값을 그대로 먹이면 커서를 빠르게 움직일 때
	//	기울기가 프레임마다 튀어 카드가 덜덜 떤다.
	//	호버가 풀리는 동안 0 으로 되돌린다. 마지막 위치가 남으면 다음에 반대쪽에서 들어올 때
	//	그 각도에서 출발해 홱 돈다.
	const FVector2D DesiredHoverLocal = bHoverAllowed ? HoverLocalNormalized : FVector2D::ZeroVector;
	HoverLocalSmoothed = (HoverFollowSpeed > 0.f)
		? FMath::Vector2DInterpTo(HoverLocalSmoothed, DesiredHoverLocal, DeltaTime, HoverFollowSpeed)
		: DesiredHoverLocal;

	//	상태가 바뀌는 순간에만 BP 로 알린다. 매 프레임 쏘면 BP 애니메이션이 계속 0 프레임으로
	//	되감겨 아무것도 재생되지 않는다.
	if (bHoverAllowed != bWasHoverActive)
	{
		bWasHoverActive = bHoverAllowed;
		RefreshChanceModifiedBanner();   // 문구는 호버가 켜지고 꺼질 때만 바뀐다
		OnHoverChanged(bHoverAllowed);
	}

	//	HoverBlend 를 머테리얼로 흘려보내지 않는다 — 외곽선 글로우의 HoverAmount 는 WBP 의
	//	UMG 애니메이션이 몬다. 여기서는 위치·기울기·크기에만 쓰인다.
}

void UBGCardWidget::ApplyMotionTransform()
{
	//	배치가 정한 값 위에 부수 효과를 더한다. 순서가 아니라 합이다 —
	//	배치와 호버가 독립적으로 계산돼야 서로를 지우지 않는다.
	const FVector2D HoverOffset(0.f, HoverLift * HoverBlend);

	//	커서가 카드 중심에서 벗어난 정도(-1~+1)에 응답 곡선을 먹인 값.
	//	기울기와 비틀림이 같은 입력을 쓰므로 둘이 따로 놀지 않는다.
	const FVector2D HoverResponse(
		BGCardMotion::ShapeResponse(HoverLocalSmoothed.X, HoverResponseExponent),
		BGCardMotion::ShapeResponse(HoverLocalSmoothed.Y, HoverResponseExponent));

	const float HoverTilt = HoverResponse.X * HoverTiltMax * HoverBlend;
	const float FinalScale = CurrentScale * FMath::Lerp(1.f, HoverScale, HoverBlend);

	//	'Slot' 은 UWidget 의 멤버명이라 지역 변수 이름을 달리한다 (C4458 은 에러로 승격됨).
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetPosition(CurrentPosition + HoverOffset);
	}

	//	캔버스가 아니면(구형 HorizontalBox 손패) 위치는 패널이 정한다 — 회전/크기만 반영된다.
	SetRenderTransformAngle(CurrentAngle + VelocityTilt + HoverTilt);
	SetRenderScale(FVector2D(FinalScale, FinalScale));

	//	UMG 는 2D 라 진짜 3D 회전은 없지만, shear 로 커서 쪽으로 눕는 인상을 만들 수 있다.
	if (HoverShearMax != 0.f)
	{
		SetRenderShear(FVector2D(
			-HoverResponse.Y * HoverShearMax * HoverBlend,
			 HoverResponse.X * HoverShearMax * HoverBlend));
	}
}

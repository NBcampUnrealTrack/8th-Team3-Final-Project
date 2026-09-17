#include "UI/Widget/BGRollGaugeWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/TextBlock.h"
#include "Core/BGGameState.h"
#include "Curves/CurveFloat.h"
#include "Debug/BGCardLog.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/Presentation/BGPresentationDirector.h"

namespace BGRollGauge
{
	/** 되감기 속도. 한 틱에 끝나 첫 프레임 상태가 적용되게 한다. */
	constexpr float RewindPlaybackSpeed = 1000.f;
}

int32 UBGRollGaugeWidget::TierToSlotIndex(EBGResolutionTier Tier)
{
	switch (Tier)
	{
	case EBGResolutionTier::CriticalFailure: return 0;
	case EBGResolutionTier::Failure:         return 1;
	case EBGResolutionTier::Success:         return 2;
	case EBGResolutionTier::CriticalSuccess: return 3;
	default:                                 return INDEX_NONE;
	}
}

void UBGRollGaugeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Border_Root)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[RollGauge] Border_Root 가 바인딩됐다 — 결과 색이 착지 전에 칠해진다. 게이지 WBP 에서는 이름을 바꿀 것."));
	}

	//	공개 연출이 폭을 바꾸기 전에, 디자이너가 잡은 원래 폭을 붙잡아 둔다.
	CaptureDesignerWeights();
}

void UBGRollGaugeWidget::NativeDestruct()
{
	ResetGauge();
	Super::NativeDestruct();
}

// ── 들어오기 ──────────────────────────────────────────────────────────────────

void UBGRollGaugeWidget::ShowResolution(const FBGResolutionView& InResolutionView)
{
	//	None 은 ABGPlayerController::ToResolutionTier 가 새 등급을 모른다는 뜻이다.
	if (!ensureMsgf(InResolutionView.IsSet(), TEXT("[RollGauge] 등급이 None 인 판정 — ToResolutionTier 확인")))
	{
		return;
	}

	//	서버가 판정 UI 동안 카드 사용을 막으므로 정상 흐름에서는 겹치지 않는다.
	//	겹쳤다면 앞 연출을 거두고 새 결과로 간다 — 앞 표를 쥔 채 두면 큐가 상한까지 멈춘다.
	if (Phase != EBGRollGaugePhase::Idle)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[RollGauge] 진행 중(%s)에 새 판정이 왔다 — 앞 연출을 끊는다."),
			*GetPhaseName());
		ResetGauge();
	}

	ResolutionView = InResolutionView;

	UE_LOG(LogBGCard, Log, TEXT("[RollGauge] 판정 수신 — %s · %s · 눈 %d%s"),
		*ResolutionView.CardName.ToString(), *GetTierText(ResolutionView.Tier).ToString(),
		ResolutionView.Roll, ResolutionView.bForced ? TEXT(" (강제)") : TEXT(""));

	//	카드 이름 · 확률은 스포일러가 아니라 미리 채운다. 결과 텍스트만 착지까지 숨긴다.
	RefreshVisual();
	UpdateResultTexts();
	ApplyResultImage();
	SetOutcomeWidgetsVisible(false);

	//	지난 판정의 공개 연출이 좁혀 둔 칸을 원래 폭으로 돌린다.
	ApplySlotWidths();

	const UWorld* World = GetWorld();
	ShowTimeSeconds = World ? World->GetTimeSeconds() : 0.;

	//	⚠️ 카드 퇴장을 기다리는 동안에도 쥔다. 안 쥐면 그 틈에 연출 큐가 끝나 버린다.
	UIHold.Begin(UBGPresentationDirector::Get(this), EBGPresentationTrack::UI);

	Phase = EBGRollGaugePhase::WaitingForCard;

	if (World)
	{
		World->GetTimerManager().SetTimer(CardExitTimeoutTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				//	손패 위젯이 없거나 카드 위젯을 못 만든 경우다. 게이지만이라도 굴린다.
				UE_LOG(LogBGCard, Log, TEXT("[RollGauge] 카드 퇴장 신호 없음 — %.1f초 상한으로 시작한다."),
					CardExitTimeoutSeconds);
				StartRoll();
			}),
			FMath::Max(CardExitTimeoutSeconds, KINDA_SMALL_NUMBER), /*bLoop=*/false);
	}
}

void UBGRollGaugeWidget::NotifyCardExitStarted()
{
	//	대기 중이 아닐 때 오는 퇴장(장비 등 판정 없는 카드)은 게이지와 무관하다.
	if (Phase == EBGRollGaugePhase::WaitingForCard)
	{
		UE_LOG(LogBGCard, Log, TEXT("[RollGauge] 카드 퇴장 신호 → 굴림 시작"));
		StartRoll();
	}
}

void UBGRollGaugeWidget::HideResolution()
{
	//	"연출이 가끔 안 보인다" 를 추적할 때 누가 언제 껐는지가 핵심 단서다.
	UE_LOG(LogBGCard, Log, TEXT("[RollGauge] 숨김 요청 — 단계 %s"), *GetPhaseName());

	//	표시가 끝났다 — 결과창을 역재생으로 접는다. 등장한 길로 되돌아가야 퇴장이 따로 논다는 느낌이 없다.
	//	⚠️ 이 단계에서만 한다. 판이 끝나거나 턴이 넘어가 끊긴 경우(Holding 이 아님)에는 즉시 감춘다.
	if (Phase == EBGRollGaugePhase::Holding && Anim_ResultIn)
	{
		Phase = EBGRollGaugePhase::Outro;

		PlayAnimation(Anim_ResultIn, 0.f, 1, EUMGSequencePlayMode::Reverse, OutroPlaybackSpeed);

		const float OutroSeconds =
			(Anim_ResultIn->GetEndTime() - Anim_ResultIn->GetStartTime()) / FMath::Max(OutroPlaybackSpeed, KINDA_SMALL_NUMBER);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(OutroTimer, this, &UBGRollGaugeWidget::FinishOutro,
				FMath::Max(OutroSeconds, KINDA_SMALL_NUMBER), /*bLoop=*/false);
			return;
		}
	}

	ResetGauge();
	Super::HideResolution();
}

void UBGRollGaugeWidget::FinishOutro()
{
	//	역재생이 끝나 결과창이 첫 프레임(투명)으로 돌아왔다. 다음 판정에서 되감을 것이 없다.
	LastResultInAnimation = nullptr;

	ResetGauge();
	Super::HideResolution();
}

// ── 진행 ──────────────────────────────────────────────────────────────────────

void UBGRollGaugeWidget::StartRoll()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CardExitTimeoutTimer);
	}

	//	⚠️ 둘 중 하나라도 없으면 화살표 좌표를 못 구해 게이지가 멈춘 채로 보인다. 이름 오타가 흔하다.
	if (!Panel_Track || !Image_Arrow)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[RollGauge] %s 바인딩이 비었다 — 화살표가 움직이지 않는다. WBP 에서 이름과 Is Variable 을 확인할 것."),
			!Panel_Track ? TEXT("Panel_Track") : TEXT("Image_Arrow"));
	}

	BeginShowing();
	RewindLastAnimations();

	//	도착점을 먼저 정한다. 이후 모든 프레임은 이 경로 위의 한 점일 뿐이다.
	const bool bForced = ResolutionView.bForced;
	CursorEnd     = PlanCursorEnd(bForced ? ForcedRollPasses : RollPasses, ComputeLandingRatio());
	Phase         = EBGRollGaugePhase::Rolling;
	PhaseElapsed  = 0.f;
	PhaseDuration = bForced ? ForcedRollSeconds : RollSeconds;
	LastArrowSlot = INDEX_NONE;
	bArrowPlaced  = false;

	//	트랙 폭을 모르는 첫 프레임에는 화살표가 레이아웃 원점에 서 있다. 첫 배치 때 드러낸다.
	//	트랙이 없으면 첫 배치가 영영 오지 않으므로 숨기지 않는다.
	if (Image_Arrow && Panel_Track)
	{
		Image_Arrow->SetRenderOpacity(0.f);
	}
}

void UBGRollGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	switch (Phase)
	{
	case EBGRollGaugePhase::Rolling: TickRolling(InDeltaTime); break;
	case EBGRollGaugePhase::Landing: TickLanding(InDeltaTime); break;
	default: break;
	}

	//	단계와 따로 돈다 — 칸 연출이 좁히기보다 짧으면 다음 단계에 들어가서도 좁히기가 남아 있다.
	if (bCollapsing)
	{
		TickCollapse(InDeltaTime);
	}
}

void UBGRollGaugeWidget::TickRolling(float DeltaTime)
{
	PhaseElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(PhaseElapsed / FMath::Max(PhaseDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

	const float Progress = RollEaseCurve
		? RollEaseCurve->GetFloatValue(Alpha)
		: 1.f - FMath::Pow(1.f - Alpha, RollEaseExponent);

	ApplyArrow(CursorToRatio(CursorEnd * Progress));

	if (Alpha >= 1.f)
	{
		//	곡선 끝값이 1 이 아니어도 도착점에는 정확히 세운다. 결과 칸을 벗어나면 안 된다.
		ApplyArrow(CursorToRatio(CursorEnd));

		WarnIfOverServerBudget();

		Phase         = EBGRollGaugePhase::Landing;
		PhaseElapsed  = 0.f;
		PhaseDuration = LandingPauseSeconds;

		OnRollLanded(ResolutionView);
		OnRollLandedEvent.Broadcast(ResolutionView);
	}
}

void UBGRollGaugeWidget::TickLanding(float DeltaTime)
{
	PhaseElapsed += DeltaTime;
	if (PhaseElapsed >= PhaseDuration)
	{
		BeginReveal();
	}
}

void UBGRollGaugeWidget::BeginReveal()
{
	Phase = EBGRollGaugePhase::Revealing;

	//	여기서부터 칸 폭을 "네 칸" → "결과 칸 하나" 로 좁혀 간다.
	//	선택 칸을 옮기지 않고 폭만 바꾸는 이유: 레이아웃이 알아서 가운데로 밀어 주고,
	//	사라지는 칸이 자리째 없어져 선택 칸을 가리지 않는다.
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		RevealStartWeights[Index] = SlotWeights[Index];
	}
	SelectedSlot     = TierToSlotIndex(ResolutionView.Tier);
	CollapseElapsed  = 0.f;
	bCollapsing      = true;

	//	C++ 이 애니메이션을 먼저 건다. BP 가 무엇을 얹든 칸은 이미 좁혀지기 시작해야 한다.
	const float RevealSeconds = PlayPhaseAnimation(Anim_Reveal, RevealFallbackSeconds, LastRevealAnimation);

	OnRevealStarted(ResolutionView);

	//	칸 연출의 꼬리에 결과창의 머리를 겹친다. 끝난 뒤에 띄우면 빈 화면이 한 박자 생긴다.
	ScheduleStep(ResultInTimer, &UBGRollGaugeWidget::BeginResultIn,
		RevealSeconds - ResultInOverlapSeconds);
}

FString UBGRollGaugeWidget::GetPhaseName() const
{
	return StaticEnum<EBGRollGaugePhase>()->GetNameStringByValue(static_cast<int64>(Phase));
}

void UBGRollGaugeWidget::BeginResultIn()
{
	Phase = EBGRollGaugePhase::ResultIn;

	UE_LOG(LogBGCard, Log, TEXT("[RollGauge] 결과창 등장 — 문구 \"%s\" · 애니메이션 %s"),
		*ResolutionView.DescriptionText.ToString(), Anim_ResultIn ? TEXT("있음") : TEXT("없음"));

	UpdateResultTexts();
	SetOutcomeWidgetsVisible(true);

	const float ResultInSeconds = PlayPhaseAnimation(Anim_ResultIn, ResultInFallbackSeconds, LastResultInAnimation);

	OnResultShown(ResolutionView);

	ScheduleStep(ResultInFinishTimer, &UBGRollGaugeWidget::FinishResultIn, ResultInSeconds);
}

// ── 단계 도우미 ───────────────────────────────────────────────────────────────

void UBGRollGaugeWidget::RewindLastAnimations()
{
	//	⚠️ 화면에 올린 **뒤**에 불러야 한다 — 접힌 위젯은 틱이 없어 애니메이션이 진행되지 않는다.
	//	   역재생으로 첫 프레임(= 평상시 상태)까지 되돌린다. 속도를 크게 줘서 한 틱에 끝낸다.
	for (TObjectPtr<UWidgetAnimation>* Last : { &LastRevealAnimation, &LastResultInAnimation })
	{
		if (*Last)
		{
			PlayAnimation(*Last, 0.f, 1, EUMGSequencePlayMode::Reverse, BGRollGauge::RewindPlaybackSpeed);
			*Last = nullptr;
		}
	}
}

float UBGRollGaugeWidget::PlayPhaseAnimation(UWidgetAnimation* Animation, float FallbackSeconds,
	TObjectPtr<UWidgetAnimation>& OutLast)
{
	if (!Animation)
	{
		return FallbackSeconds;   // WBP 가 이 단계 연출을 두지 않았다. 시간만 흘려보낸다.
	}

	PlayAnimation(Animation);
	OutLast = Animation;

	return Animation->GetEndTime() - Animation->GetStartTime();
}

void UBGRollGaugeWidget::ScheduleStep(FTimerHandle& Timer, void (UBGRollGaugeWidget::*Callback)(), float Seconds)
{
	UWorld* World = GetWorld();
	if (World && Seconds > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(Timer, this, Callback, Seconds, /*bLoop=*/false);
		return;
	}

	//	기다릴 것이 없다(연출이 없거나 이미 지났다). 다음 단계로 바로 넘어간다.
	(this->*Callback)();
}

void UBGRollGaugeWidget::TickCollapse(float DeltaTime)
{
	CollapseElapsed += DeltaTime;
	const float Alpha = RevealCollapseSeconds > 0.f
		? FMath::Clamp(CollapseElapsed / RevealCollapseSeconds, 0.f, 1.f)
		: 1.f;

	const float Shape = RevealCollapseCurve
		? RevealCollapseCurve->GetFloatValue(Alpha)
		: FMath::InterpEaseOut(0.f, 1.f, Alpha, 3.f);

	//	⚠️ 여기서 RenderTransform · Opacity 를 건드리지 않는다. 그쪽은 Anim_Reveal 의 몫이다 —
	//	   양쪽이 같은 값을 쓰면 프레임마다 서로 덮어써서 애니메이션 키가 안 먹는다.
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		const float Target = Index == SelectedSlot ? 1.f : 0.f;
		SlotWeights[Index] = FMath::Lerp(RevealStartWeights[Index], Target, Shape);
	}

	PushSlotWidths();

	if (Alpha >= 1.f)
	{
		bCollapsing = false;
	}
}

void UBGRollGaugeWidget::FinishResultIn()
{
	Phase = EBGRollGaugePhase::Holding;

	UIHold.End(UBGPresentationDirector::Get(this));

	//	결과를 DisplaySeconds 만큼 더 보여 주고 부모의 자동 숨김으로 내린다.
	StartAutoHideTimer();
}

void UBGRollGaugeWidget::ResetGauge()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CardExitTimeoutTimer);
		World->GetTimerManager().ClearTimer(ResultInTimer);
		World->GetTimerManager().ClearTimer(ResultInFinishTimer);
		World->GetTimerManager().ClearTimer(OutroTimer);
	}

	//	⚠️ 부모의 자동 숨김도 지운다. 남겨 두면 앞 판정의 타이머가 다음 판정의 굴림 도중에 발동해
	//	   연출을 통째로 끈다 — 카드를 연달아 낼 때만 가끔 연출이 안 뜨던 원인이었다.
	ClearAutoHideTimer();

	//	퇴장 중에 끊겼다(새 판정 · 판 종료). 역재생을 멈추지 않으면 다음 판정의 등장과 겹쳐 돈다.
	if (Phase == EBGRollGaugePhase::Outro && Anim_ResultIn)
	{
		StopAnimation(Anim_ResultIn);
	}

	//	⚠️ 어느 단계에서 끊겨도 표를 놓는다. 쥔 채 두면 연출 큐가 상한(8초)까지 멈춘다.
	UIHold.End(UBGPresentationDirector::Get(this));

	Phase         = EBGRollGaugePhase::Idle;
	PhaseElapsed  = 0.f;
	LastArrowSlot = INDEX_NONE;
	bCollapsing   = false;

	SetOutcomeWidgetsVisible(false);
}

// ── 굴림 경로 ─────────────────────────────────────────────────────────────────

float UBGRollGaugeWidget::CursorToRatio(float Cursor)
{
	const float Wrapped = FMath::Fmod(FMath::Max(Cursor, 0.f), 2.f);
	return Wrapped <= 1.f ? Wrapped : 2.f - Wrapped;
}

float UBGRollGaugeWidget::PlanCursorEnd(float MinPasses, float LandingRatio)
{
	//	비율 r 에 닿는 진행량은 두 계열뿐이다 — → 방향으로 닿는 2m + r, ← 방향으로 닿는 2m + 2 - r.
	//	최소 왕복 수를 넘기는 것 중 가장 가까운 것을 고르면 매번 비슷한 길이로 돈다.
	const float Ratio = FMath::Clamp(LandingRatio, 0.f, 1.f);

	for (int32 Pair = FMath::Max(0, FMath::FloorToInt(MinPasses * 0.5f) - 1); ; ++Pair)
	{
		const float Rightward = 2.f * Pair + Ratio;
		if (Rightward >= MinPasses)
		{
			return Rightward;
		}

		const float Leftward = 2.f * Pair + 2.f - Ratio;
		if (Leftward >= MinPasses)
		{
			return Leftward;
		}
	}
}

float UBGRollGaugeWidget::ComputeLandingRatio() const
{
	const int32 LandingSlot = TierToSlotIndex(ResolutionView.Tier);
	if (LandingSlot == INDEX_NONE)
	{
		return 0.5f;
	}

	float SlotStart = 0.f;
	float SlotEnd = 0.f;
	GetSlotRange(LandingSlot, SlotStart, SlotEnd);

	//	칸 안의 자리. 강제 판정이나 확률이 없으면 칸 가운데다.
	float Fraction = 0.5f;

	const FBGCardChanceView& Chance = ResolutionView.Chance;
	if (!ResolutionView.bForced && ResolutionView.Roll != INDEX_NONE && Chance.IsSet())
	{
		//	RollOutcome 과 같은 누적 순서(대실패 → 대성공)라 Roll 이 곧 왼쪽부터의 %다.
		const float Percents[SlotCount] = { Chance.CriticalFailure, Chance.Failure, Chance.Success, Chance.CriticalSuccess };

		float BandStart = 0.f;
		for (int32 Index = 0; Index < LandingSlot; ++Index)
		{
			BandStart += Percents[Index];
		}

		if (Percents[LandingSlot] > KINDA_SMALL_NUMBER)
		{
			//	+0.5 = 눈 한 칸의 가운데. 눈 0 이 칸 왼쪽 끝에 딱 붙지 않게 한다.
			Fraction = (ResolutionView.Roll + 0.5f - BandStart) / Percents[LandingSlot];
			Fraction = FMath::Clamp(Fraction, RollInSlotMargin, 1.f - RollInSlotMargin);
		}
	}

	return FMath::Lerp(SlotStart, SlotEnd, Fraction);
}

// ── 칸 ────────────────────────────────────────────────────────────────────────

void UBGRollGaugeWidget::CaptureDesignerWeights()
{
	//	WBP 에서 잡아 둔 칸 폭(Fill 값)을 기억한다. 칸 폭은 확률과 무관하게 이 값으로 고정이다.
	//	Fill 이 아니면(Auto 등) 폭을 비율로 알 수 없으므로 똑같이 나눈 것으로 본다.
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		DesignerWeights[Index] = 1.f;

		const UWidget* TrackChild = GetTrackChild(Index);
		if (!TrackChild)
		{
			continue;   // 이 칸을 WBP 에 두지 않았다. 선택 사항이다.
		}

		const UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(TrackChild->Slot);
		if (!BoxSlot)
		{
			UE_LOG(LogBGCard, Warning,
				TEXT("[RollGauge] %s 가 Panel_Track(HorizontalBox) 아래에 없다 — 칸 폭 · 화살표 위치가 어긋난다."),
				*TrackChild->GetName());
			continue;
		}

		const FSlateChildSize Size = BoxSlot->GetSize();
		if (Size.SizeRule == ESlateSizeRule::Fill && Size.Value > KINDA_SMALL_NUMBER)
		{
			DesignerWeights[Index] = Size.Value;
		}
		else
		{
			//	Auto 면 실제 폭은 내용물이 정하는데 화살표는 4등분으로 계산한다 — 둘이 어긋난다.
			UE_LOG(LogBGCard, Warning,
				TEXT("[RollGauge] %s 의 Size 가 Fill 이 아니다 — 화살표가 칸과 어긋난다. WBP 에서 Fill 로 둘 것."),
				*TrackChild->GetName());
		}
	}
}

void UBGRollGaugeWidget::ApplySlotWidths()
{
	//	⚠️ 매 판정 되돌려 놓는다. 지난 공개 연출이 결과 칸 하나로 좁혀 둔 채 끝났기 때문이다.
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		SlotWeights[Index] = DesignerWeights[Index];
	}

	PushSlotWidths();
}

void UBGRollGaugeWidget::PushSlotWidths()
{
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		UWidget* TrackChild = GetTrackChild(Index);
		if (!TrackChild)
		{
			continue;
		}

		//	경고는 CaptureDesignerWeights 에서 한 번만 한다. 여기는 좁히기 동안 매 프레임 불린다.
		UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(TrackChild->Slot);
		if (!BoxSlot)
		{
			continue;
		}

		//	⚠️ 화살표 좌표도 SlotWeights 로 계산한다. 다른 값을 넣으면 화살표가 칸과 어긋난다.
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = SlotWeights[Index];
		BoxSlot->SetSize(Size);
	}
}

UWidget* UBGRollGaugeWidget::GetTrackChild(int32 SlotIndex) const
{
	//	폭을 바꿀 대상은 Panel_Track 의 **직계 자식**이다. Slot_* 을 Overlay 로 감싸 두는 구성이
	//	흔하므로(배경 + 아이콘 + 발광), 바인딩된 위젯에서 트랙까지 거슬러 올라가 그 자식을 찾는다.
	UWidget* Widget = GetSlotWidget(SlotIndex);
	UPanelWidget* Track = Cast<UPanelWidget>(Panel_Track);
	if (!Widget || !Track)
	{
		return Widget;
	}

	while (Widget && Widget->GetParent() != Track)
	{
		Widget = Widget->GetParent();
	}

	//	트랙 아래에 없다(이름을 엉뚱한 위젯에 붙였다). 원래 위젯을 돌려주고 위에서 경고하게 둔다.
	return Widget ? Widget : GetSlotWidget(SlotIndex);
}

void UBGRollGaugeWidget::GetSlotRange(int32 SlotIndex, float& OutStart, float& OutEnd) const
{
	float Total = 0.f;
	float Before = 0.f;
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		if (Index < SlotIndex)
		{
			Before += SlotWeights[Index];
		}
		Total += SlotWeights[Index];
	}

	const float Inv = Total > KINDA_SMALL_NUMBER ? 1.f / Total : 0.f;
	OutStart = Before * Inv;
	OutEnd   = (SlotIndex >= 0 && SlotIndex < SlotCount) ? (Before + SlotWeights[SlotIndex]) * Inv : OutStart;
}

int32 UBGRollGaugeWidget::TrackRatioToSlotIndex(float Ratio) const
{
	for (int32 Index = 0; Index < SlotCount - 1; ++Index)
	{
		float Start = 0.f;
		float End = 0.f;
		GetSlotRange(Index, Start, End);
		if (Ratio < End)
		{
			return Index;
		}
	}
	return SlotCount - 1;
}

void UBGRollGaugeWidget::UpdateResultTexts()
{
	if (Text_Effect)
	{
		//	카드가 DT 에 적어 둔 문구를 그대로 쓴다. 안 적었으면 비운다 —
		//	자동 요약으로 대신 채우면 "Status.NextCard.ForceFailure" 같은 내부 문자열이 화면에 샌다.
		//	수치를 넣고 싶은 카드는 DT 문구에 {Amount} · {Effect} 를 쓰면 된다.
		Text_Effect->SetText(ResolutionView.DescriptionText);
	}

	//	부모가 채워 둔 기본 문구("대성공")를 게이지 전용 문구로 덮는다. 비워 두면 기본값 그대로다.
	if (Text_Outcome)
	{
		const FText& Override = GetOutcomeTextOverride(TierToSlotIndex(ResolutionView.Tier));
		if (!Override.IsEmpty())
		{
			Text_Outcome->SetText(Override);
		}
	}
}

// ── 화살표 ────────────────────────────────────────────────────────────────────

bool UBGRollGaugeWidget::ApplyArrow(float Ratio)
{
	const int32 SlotIndex = TrackRatioToSlotIndex(Ratio);
	if (SlotIndex != LastArrowSlot)
	{
		LastArrowSlot = SlotIndex;
		OnArrowSlotChanged(SlotIndex);
	}

	if (!Image_Arrow || !Panel_Track)
	{
		return false;
	}

	const float TrackWidth = Panel_Track->GetCachedGeometry().GetLocalSize().X;
	if (TrackWidth <= KINDA_SMALL_NUMBER)
	{
		return false;   // 이번 프레임에 막 화면에 올라왔다. 다음 틱에 세운다.
	}

	//	화살표의 레이아웃 원점이 트랙 왼쪽 끝이라는 전제다(헤더의 배치 조건).
	//	DesiredSize 는 RenderTransform 의 영향을 받지 않아 펀치 연출 중에도 흔들리지 않는다.
	const float HalfArrowWidth = Image_Arrow->GetDesiredSize().X * 0.5f;
	const FVector2D Translation = Image_Arrow->GetRenderTransform().Translation;

	Image_Arrow->SetRenderTranslation(FVector2D(Ratio * TrackWidth - HalfArrowWidth, Translation.Y));

	if (!bArrowPlaced)
	{
		bArrowPlaced = true;
		Image_Arrow->SetRenderOpacity(1.f);
	}
	return true;
}

void UBGRollGaugeWidget::SetOutcomeWidgetsVisible(bool bVisible)
{
	//	Hidden 이라 자리는 남는다. 공개 순간 레이아웃이 밀리면 확대 연출이 흔들린다.
	const ESlateVisibility OutcomeVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;

	//	배너는 게이지 위에 겹쳐 두므로 자리를 남길 이유가 없다. 숨길 때는 Collapsed.
	if (Panel_Result)
	{
		Panel_Result->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Text_Outcome)
	{
		Text_Outcome->SetVisibility(OutcomeVisibility);
	}
	//	굴린 눈은 보여 주지 않는다. WBP 에 남아 있으면 부모가 채운 "눈 N / 성공 N%" 가 뜨므로 접어 둔다.
	if (Text_Roll)
	{
		Text_Roll->SetVisibility(ESlateVisibility::Collapsed);
	}

	//	카드가 문구를 안 적었으면 줄째로 접는다. 빈 줄이 남으면 배너 가운데가 비어 보인다.
	if (Text_Effect)
	{
		const bool bHasText = !ResolutionView.DescriptionText.IsEmpty();
		Text_Effect->SetVisibility(bVisible && bHasText ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UBGRollGaugeWidget::ApplyResultImage()
{
	if (!Image_Result)
	{
		return;
	}

	UTexture2D* Texture = GetResultImage(TierToSlotIndex(ResolutionView.Tier));

	//	bMatchSize=false — WBP 에서 잡은 배너 크기를 지킨다. 텍스처 해상도가 달라도 레이아웃이 안 흔들린다.
	//	비어 있으면 WBP 에 지정한 그림을 그대로 둔다(아직 안 넣은 등급에서도 화면이 비지 않는다).
	if (Texture)
	{
		Image_Result->SetBrushFromTexture(Texture, /*bMatchSize=*/false);
	}
}

UWidget* UBGRollGaugeWidget::GetSlotWidget(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case 0:  return Slot_CriticalFailure;
	case 1:  return Slot_Failure;
	case 2:  return Slot_Success;
	case 3:  return Slot_CriticalSuccess;
	default: return nullptr;
	}
}

UTexture2D* UBGRollGaugeWidget::GetResultImage(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case 0:  return ResultImageCriticalFailure;
	case 1:  return ResultImageFailure;
	case 2:  return ResultImageSuccess;
	case 3:  return ResultImageCriticalSuccess;
	default: return nullptr;
	}
}

const FText& UBGRollGaugeWidget::GetOutcomeTextOverride(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case 0:  return OutcomeTextCriticalFailure;
	case 1:  return OutcomeTextFailure;
	case 2:  return OutcomeTextSuccess;
	case 3:  return OutcomeTextCriticalSuccess;
	default: return FText::GetEmpty();
	}
}


void UBGRollGaugeWidget::WarnIfOverServerBudget() const
{
	const UWorld* World = GetWorld();
	const ABGGameState* GS = World ? World->GetGameState<ABGGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	//	클라는 판정 통지를 RPC 지연만큼 늦게 받으므로 실제 여유는 이보다 적다.
	const double Elapsed = World->GetTimeSeconds() - ShowTimeSeconds;
	const float Budget   = GS->GetResolutionPresentationSeconds();
	if (Elapsed > Budget)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[RollGauge] 착지까지 %.2f초 — 서버 판정 UI 시간(%.2f초)을 넘었다. 월드 연출이 먼저 터져 결과가 드러난다. "
			     "카드 DwellDuration 이나 RollSeconds 를 줄이거나 ResolutionPresentationSeconds 를 늘릴 것."),
			Elapsed, Budget);
	}
}

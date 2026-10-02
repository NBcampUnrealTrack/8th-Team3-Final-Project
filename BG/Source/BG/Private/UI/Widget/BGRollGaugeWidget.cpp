#include "UI/Widget/BGRollGaugeWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Audio/BGAudioSubsystem.h"
#include "BGGameplayTags.h"
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

	//	지난 판정에서 켜 둔 칸을 끈다. 굴림이 시작되면 첫 칸부터 다시 들어온다.
	SetLitSlot(INDEX_NONE);

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

	//	⚠️ 하나도 없으면 불이 들어올 곳이 없어 게이지가 꺼진 채로 보인다. 이름 오타가 흔하다.
	if (!Light_CriticalFailure && !Light_Failure && !Light_Success && !Light_CriticalSuccess)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[RollGauge] Light_* 바인딩이 하나도 없다 — 불이 들어오지 않는다. WBP 에서 이름을 확인할 것."));
	}

	BeginShowing();
	RewindLastAnimations();

	//	도착점을 먼저 정한다. 이후 모든 프레임은 이 경로 위의 한 점일 뿐이다.
	const bool bForced = ResolutionView.bForced;
	CursorEnd     = PlanCursorEnd(bForced ? ForcedRollPasses : RollPasses, ComputeLandingRatio());
	Phase         = EBGRollGaugePhase::Rolling;
	PhaseElapsed  = 0.f;
	//	PvE 배율 — 서버 판정 대기가 같은 비율로 줄어 있다(ABGGameState::GetPresentationTimeScale).
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	PhaseDuration = (bForced ? ForcedRollSeconds : RollSeconds) * (GS ? GS->GetPresentationTimeScale() : 1.f);
	SetLitSlot(INDEX_NONE);
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
}

void UBGRollGaugeWidget::TickRolling(float DeltaTime)
{
	PhaseElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(PhaseElapsed / FMath::Max(PhaseDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

	const float Progress = RollEaseCurve
		? RollEaseCurve->GetFloatValue(Alpha)
		: 1.f - FMath::Pow(1.f - Alpha, RollEaseExponent);

	ApplyLights(CursorToRatio(CursorEnd * Progress));

	if (Alpha >= 1.f)
	{
		//	곡선 끝값이 1 이 아니어도 도착점에는 정확히 맞춘다. 결과 칸을 벗어나면 안 된다.
		ApplyLights(CursorToRatio(CursorEnd));

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

	//	착지 때 이미 결과 칸이 켜져 있다 — 그대로 둔다. 코드가 여기서 더 할 일은 없다.
	//	칸 펀치 · 화면 플래시 같은 덧칠은 Anim_Reveal 과 OnRevealStarted 의 몫이다.
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

	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		switch (ResolutionView.Tier)
		{
		case EBGResolutionTier::CriticalFailure:
			Audio->Play(BG::Audio::UI::RollGauge::Result::CriticalFailure.GetTag());
			break;
		case EBGResolutionTier::Failure:
			Audio->Play(BG::Audio::UI::RollGauge::Result::Failure.GetTag());
			break;
		case EBGResolutionTier::Success:
			Audio->Play(BG::Audio::UI::RollGauge::Result::Success.GetTag());
			break;
		case EBGResolutionTier::CriticalSuccess:
			Audio->Play(BG::Audio::UI::RollGauge::Result::CriticalSuccess.GetTag());
			break;
		default:
			break;
		}
	}

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

	Phase        = EBGRollGaugePhase::Idle;
	PhaseElapsed = 0.f;

	SetLitSlot(INDEX_NONE);
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

void UBGRollGaugeWidget::GetSlotRange(int32 SlotIndex, float& OutStart, float& OutEnd) const
{
	//	칸은 균등 4등분이다 — 스트립 그림에 네 자리가 균등하게 박혀 있기 때문이다.
	//	확률과는 무관하다. 어느 칸에 서는지는 Tier 가, 칸 안 어디에 서는지는 Roll 이 정한다.
	const float Width = 1.f / SlotCount;

	OutStart = FMath::Clamp(SlotIndex, 0, SlotCount - 1) * Width;
	OutEnd   = OutStart + Width;
}

int32 UBGRollGaugeWidget::TrackRatioToSlotIndex(float Ratio) const
{
	//	비율 1.0(오른쪽 끝)은 마지막 칸에 넣는다 — 안 그러면 칸 밖 번호가 나온다.
	return FMath::Clamp(FMath::FloorToInt(Ratio * SlotCount), 0, SlotCount - 1);
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

// ── 점등 ──────────────────────────────────────────────────────────────────────

void UBGRollGaugeWidget::ApplyLights(float Ratio)
{
	SetLitSlot(TrackRatioToSlotIndex(Ratio));
}

void UBGRollGaugeWidget::SetLitSlot(int32 SlotIndex)
{
	//	굴림 중에는 매 프레임 불리지만 칸이 바뀌는 건 몇 번뿐이다. 같은 칸이면 그냥 나간다 —
	//	안 그러면 BP 이벤트가 프레임마다 터져 틱 사운드가 굉음이 된다.
	if (SlotIndex == LitSlot)
	{
		return;
	}

	LitSlot = SlotIndex;

	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		if (UWidget* Light = GetLightWidget(Index))
		{
			Light->SetRenderOpacity(Index == SlotIndex ? 1.f : 0.f);
		}
	}

	//	전부 끄는 경우(판정 사이)는 연출이 아니다. BP 에 알리지 않는다.
	if (SlotIndex != INDEX_NONE)
	{
		if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
		{
			Audio->Play(BG::Audio::UI::RollGauge::LightStep.GetTag());
		}
		OnLitSlotChanged(SlotIndex);
	}
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

UWidget* UBGRollGaugeWidget::GetLightWidget(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case 0:  return Light_CriticalFailure;
	case 1:  return Light_Failure;
	case 2:  return Light_Success;
	case 3:  return Light_CriticalSuccess;
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

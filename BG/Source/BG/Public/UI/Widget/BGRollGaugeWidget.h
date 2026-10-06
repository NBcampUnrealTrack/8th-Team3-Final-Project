// 판정 게이지 — 대실패 · 실패 · 성공 · 대성공 4칸에 신호등처럼 불이 차례로 들어오다
// 굴린 눈(Roll) 이 가리키는 칸에서 멈춘다.
//
// 결과는 서버가 이미 정했다. 이 위젯은 그 자리로 가는 척만 한다 — 클라에서 다시 굴리지 않는다.
//
// 그림은 "꺼진 스트립 한 장(Image_GaugeOff) + 칸별 발광 한 장(Light_*)" 을 겹쳐 둔 것이다.
// 다섯 장 모두 같은 캔버스라 Overlay 에 쌓기만 하면 자리가 맞는다 — 점등은 Light_* 의
// RenderOpacity 를 0/1 로 바꾸는 것이 전부다.
//
// 움직임은 룰렛 방식이다. 도착점을 먼저 정하고 "왕복 N번 + 도착점까지" 의 총 이동량을 하나의
// 감속 곡선으로 소화한다. 왕복 → 착지를 따로 굴리면 둘 사이에서 속도가 꺾여 연출이 들킨다.
// 커서는 연속값이고, 그 값이 들어온 칸에 불을 켠다. 칸은 똑같이 4등분이다.
//
// 흐름: ShowResolution(결과 보관) → 카드 퇴장 신호 → Rolling(불이 오감) → Landing(히트스톱)
//       → Revealing(당첨 칸만 켠 채 유지) → ResultIn(결과창 등장) → Holding → Outro(역재생) → Hide
//
// ── 누가 무엇을 소유하는가 ────────────────────────────────────────────────────
//   코드 : Light_* 의 RenderOpacity, 결과창 그림 · 문구, 단계 전환
//   WBP  : 결과창의 RenderTransform · Opacity (Anim_Reveal / Anim_ResultIn)
//   ⚠️ 겹치면 프레임마다 서로 덮어써 애니메이션 키가 안 먹는다. 코드는 위 목록 밖을 건드리지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/BGResolutionWidget.h"
#include "UI/Presentation/BGPresentationTypes.h"
#include "BGRollGaugeWidget.generated.h"

class UWidget;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidgetAnimation;
class UCurveFloat;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGRollGaugeLandedEvent, const FBGResolutionView&, Landed);

UENUM(BlueprintType)
enum class EBGRollGaugePhase : uint8
{
	Idle            UMETA(DisplayName = "대기 없음"),
	WaitingForCard  UMETA(DisplayName = "카드 퇴장 대기"),
	Rolling         UMETA(DisplayName = "굴림"),
	Landing         UMETA(DisplayName = "착지 정지"),
	Revealing       UMETA(DisplayName = "칸 연출"),
	ResultIn        UMETA(DisplayName = "결과창 등장"),
	Outro           UMETA(DisplayName = "결과창 퇴장"),
	Holding         UMETA(DisplayName = "표시 유지"),
};

UCLASS(Abstract, Blueprintable)
class BG_API UBGRollGaugeWidget : public UBGResolutionWidget
{
	GENERATED_BODY()

public:
	virtual void ShowResolution(const FBGResolutionView& InResolutionView) override;
	virtual void HideResolution() override;
	virtual void NotifyCardExitStarted() override;

	UFUNCTION(BlueprintPure, Category = "BG|RollGauge")
	EBGRollGaugePhase GetPhase() const { return Phase; }

	/** 등급 → 칸 번호(0 = 대실패 … 3 = 대성공). None 이면 INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "BG|RollGauge")
	static int32 TierToSlotIndex(EBGResolutionTier Tier);

	/** 화살표가 멈춘 순간(히트스톱 시작). HUD 등 바깥 구독용 — OnRollLanded 와 같은 순간이다. */
	UPROPERTY(BlueprintAssignable, Category = "BG|RollGauge")
	FOnBGRollGaugeLandedEvent OnRollLandedEvent;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	/** 불이 다른 칸으로 넘어갔다. 틱 사운드 · 추가 연출용. 점등 자체는 코드가 한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|RollGauge")
	void OnLitSlotChanged(int32 SlotIndex);

	/** 멈췄다. 착지음 · 당첨 칸 펀치를 여기에. 이 뒤 LandingPauseSeconds 만큼 정지하고 공개한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|RollGauge")
	void OnRollLanded(const FBGResolutionView& Landed);

	/** 칸 연출이 시작됐다. Reveal 애니메이션은 이미 시작했고, 당첨 칸은 켜진 채로 남아 있다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|RollGauge")
	void OnRevealStarted(const FBGResolutionView& Revealed);

	/** 결과창이 뜨기 시작했다. 결과 사운드 · 화면 플래시를 여기에. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|RollGauge")
	void OnResultShown(const FBGResolutionView& Shown);

	// ── 시간 ─────────────────────────────────────────────────────────
	//	월드 연출은 판정 UI 종료 통지 뒤 시작한다. 통지 누락 때만 GameState 안전망이 작동한다.

	/** 카드 퇴장 신호를 기다리는 상한. 넘으면 신호 없이 시작한다. 카드 대기 시간보다 길어야 한다. */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float CardExitTimeoutSeconds = 3.f;

	/** 굴림 전체 길이(출발 → 정지). */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float RollSeconds = 1.5f;

	/** 준비·함정으로 강제된 판정. 굴림이 아니므로 짧게 끝낸다. */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float ForcedRollSeconds = 0.5f;

	/** 멈춘 뒤 공개 전 정지(히트스톱). 이 짧은 멈춤이 "딱 걸렸다" 로 읽힌다. */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float LandingPauseSeconds = 0.12f;

	/** Reveal 애니메이션이 없을 때 칸 연출에 주는 시간. */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float RevealFallbackSeconds = 0.6f;

	/**
	 * Reveal 이 끝나기 이만큼 전에 결과창을 띄운다. 0 이면 딱 끝난 뒤에 뜬다.
	 * 칸이 사라지는 꼬리와 결과창이 튀어나오는 머리가 겹쳐야 한 동작으로 이어져 보인다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float ResultInOverlapSeconds = 0.1f;

	/** Anim_ResultIn 이 없을 때 결과창을 띄운 뒤 표시 유지로 넘어가기까지의 시간. */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float ResultInFallbackSeconds = 0.3f;

	/** 표시가 끝나면 Anim_ResultIn 을 이 배속으로 역재생해 결과창을 접는다. 1 이면 등장과 같은 길이. */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Timing", meta = (ClampMin = "0.1"))
	float OutroPlaybackSpeed = 1.5f;

	// ── 굴림 ─────────────────────────────────────────────────────────

	/** 멈추기 전까지 최소 몇 번 편도(끝→끝)로 오가는가. 실제로는 도착점에 맞춰 조금 더 간다. */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Roll", meta = (ClampMin = "0.0"))
	float RollPasses = 6.f;

	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Roll", meta = (ClampMin = "0.0"))
	float ForcedRollPasses = 1.f;

	/**
	 * 감속 세기. 진행도 = 1 - (1-t)^Exponent. 클수록 초반이 빠르고 끝에서 오래 기어간다.
	 * RollEaseCurve 를 넣으면 무시된다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Roll", meta = (ClampMin = "1.0"))
	float RollEaseExponent = 4.f;

	/** 진행도 곡선(입력 시간 0~1 → 출력 진행도 0~1). 끝값은 1 이어야 정확히 도착점에 멈춘다. */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Roll")
	TObjectPtr<UCurveFloat> RollEaseCurve;

	// ── 칸 ───────────────────────────────────────────────────────────

	//	칸은 똑같이 4등분이다 — 스트립 그림에 네 자리가 균등하게 박혀 있기 때문이다.
	//	커서는 결과 칸 안에서 "그 등급 구간 속 눈의 상대 위치" 에 멈춘다. 멈추는 자리가
	//	칸 안 어디든 켜지는 그림은 같지만, 경계에 바싹 붙으면 한 칸 옆이 켜질 수 있어 여백을 둔다.

	/**
	 * 칸 가장자리에서 이만큼(칸 폭 대비) 안쪽으로만 멈춘다.
	 * ⚠️ 표시 확률은 반올림값이라 경계에 걸린 눈이 옆 칸을 가리킬 수 있다. 등급의 진실은 Tier 다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Layout", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float RollInSlotMargin = 0.1f;

	// ── 결과 공개 ────────────────────────────────────────────────────

	//	공개는 "당첨 칸만 켠 채 그대로 둔다" 가 전부다 — 착지 때 이미 그 칸이 켜져 있으므로
	//	코드가 더 할 일이 없다. 화면 플래시 · 칸 펀치 같은 덧칠은 Anim_Reveal 의 몫이다.

	//	Text_Outcome 문구. 비워 두면 GetTierText 의 기본값("대성공" 등)을 쓴다 —
	//	여기서만 덮어쓰는 이유는 채팅 로그가 같은 GetTierText 를 쓰기 때문이다(로그까지 "대성공!" 이 되면 어색하다).

	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Reveal|Text")
	FText OutcomeTextCriticalFailure;

	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Reveal|Text")
	FText OutcomeTextFailure;

	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Reveal|Text")
	FText OutcomeTextSuccess;

	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Reveal|Text")
	FText OutcomeTextCriticalSuccess;

	//	등급별 결과창 그림. Image_Result 한 장의 브러시만 바꾼다 — 결과창을 넷으로 나누면
	//	나중에 배너를 고칠 때 네 군데를 고쳐야 하고, 한 곳을 잊으면 그 등급만 낡은 채로 남는다.

	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Reveal")
	TObjectPtr<UTexture2D> ResultImageCriticalFailure;

	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Reveal")
	TObjectPtr<UTexture2D> ResultImageFailure;

	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Reveal")
	TObjectPtr<UTexture2D> ResultImageSuccess;

	UPROPERTY(EditAnywhere, Category = "BG|RollGauge|Reveal")
	TObjectPtr<UTexture2D> ResultImageCriticalSuccess;

	// ── 바인딩 ───────────────────────────────────────────────────────

	//	칸별 발광 그림. 평소 RenderOpacity 0 이고, 불이 들어온 칸만 1 이 된다.
	//	다섯 장(꺼진 스트립 + 발광 넷)이 같은 캔버스라 Overlay 에 겹쳐 두기만 하면 된다.

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Light_CriticalFailure;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Light_Failure;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Light_Success;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Light_CriticalSuccess;

	/** 결과창 묶음. 결과창 등장 때 켜진다(평소 Collapsed). Anim_ResultIn 은 이것을 움직인다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Result;

	/** 결과창 그림 한 장. 등급에 맞는 텍스처(ResultImage*)로 코드가 바꾼다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Result;

	/** 결과 한 줄 — DT_BGCard 의 OutcomeTexts 문구 그대로. 안 적은 카드는 이 줄이 접힌다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Effect;

	/**
	 * 칸 연출(등급 공통). 점등은 코드가 하므로 여기서는 프레임 · 화면 전체처럼
	 * 등급과 무관한 것만 다룬다.
	 * 길이가 곧 칸 연출 시간이다 — 없으면 RevealFallbackSeconds 를 쓴다.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Reveal;

	/** 결과창 등장. 등급에 따라 달라지는 것(그림 · 문구)은 코드가 채우므로 하나면 된다. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_ResultIn;

private:
	static constexpr int32 SlotCount = 4;

	// ── 단계 ─────────────────────────────────────────────────────────

	void StartRoll();
	void TickRolling(float DeltaTime);
	void TickLanding(float DeltaTime);
	void BeginReveal();
	void BeginResultIn();
	void FinishResultIn();
	void FinishOutro();

	/** 진행 중인 연출을 전부 거둔다. 타이머 · 표 · 결과 텍스트. */
	void ResetGauge();

	/** 지난 판정이 남긴 애니메이션 상태를 첫 프레임으로 되감는다. 화면에 올린 뒤 부를 것. */
	void RewindLastAnimations();

	/**
	 * 애니메이션을 재생하고 길이(초)를 돌려준다. 없으면 FallbackSeconds.
	 * 재생한 것은 OutLast 에 적어 둔다 — 다음 판정에서 되감을 대상이다.
	 */
	float PlayPhaseAnimation(UWidgetAnimation* Animation, float FallbackSeconds,
		TObjectPtr<UWidgetAnimation>& OutLast);

	/** Seconds 뒤에 Callback 을 부른다. 월드가 없거나 시간이 0 이면 그 자리에서 부른다. */
	void ScheduleStep(FTimerHandle& Timer, void (UBGRollGaugeWidget::*Callback)(), float Seconds);

	// ── 점등 ─────────────────────────────────────────────────────────

	/** SlotIndex 칸만 켜고 나머지는 끈다. INDEX_NONE 이면 전부 끈다. */
	void SetLitSlot(int32 SlotIndex);

	// ── 결과 표시 ────────────────────────────────────────────────────

	void UpdateResultTexts();

	/** 칸의 트랙 비율 경계(0~1). 균등 4등분이라 레이아웃을 기다리지 않는다. */
	void GetSlotRange(int32 SlotIndex, float& OutStart, float& OutEnd) const;

	int32 TrackRatioToSlotIndex(float Ratio) const;

	/** 화살표가 멈출 트랙 비율(0~1) — 결과 칸 안에서 Roll 이 차지하는 자리. */
	float ComputeLandingRatio() const;

	/** 왕복 진행량(편도 1 = 1.0) → 트랙 비율. 짝수 구간은 → 방향, 홀수 구간은 ← 방향이다. */
	static float CursorToRatio(float Cursor);

	/** 출발점에서 RollPasses 이상 오간 뒤 LandingRatio 에 닿는 가장 가까운 총 진행량. */
	static float PlanCursorEnd(float MinPasses, float LandingRatio);

	/** 트랙 비율이 가리키는 칸에 불을 옮긴다. 칸이 바뀐 프레임에만 실제로 손댄다. */
	void ApplyLights(float Ratio);

	void SetOutcomeWidgetsVisible(bool bVisible);

	/** 결과창 그림을 이번 등급의 것으로 바꾼다. */
	void ApplyResultImage();

	//	등급이 아니라 **칸 번호**로 찾는다 — 등급 → 칸 변환을 TierToSlotIndex 한 곳으로 모으기 위함이다.
	UWidget* GetLightWidget(int32 SlotIndex) const;
	UTexture2D* GetResultImage(int32 SlotIndex) const;
	const FText& GetOutcomeTextOverride(int32 SlotIndex) const;

	/** 로그용 단계 이름. */
	FString GetPhaseName() const;

	/** 착지가 서버의 판정 UI 시간을 넘었는지 확인한다. */
	void WarnIfOverServerBudget() const;

	EBGRollGaugePhase Phase = EBGRollGaugePhase::Idle;

	float PhaseElapsed  = 0.f;
	float PhaseDuration = 0.f;

	/** 이번 굴림의 총 진행량. 감속 곡선이 0 → 이 값을 소화한다. */
	float CursorEnd = 0.f;

	/** 지금 켜져 있는 칸. 칸이 바뀐 프레임에만 위젯을 건드리기 위한 것이다. */
	int32 LitSlot = INDEX_NONE;

	/** 판정을 받은 시각(월드 초). 서버 시간 초과 경고용. */
	double ShowTimeSeconds = 0.;

	/** UI 트랙 표. 판정을 받는 순간 쥐고, 결과 공개가 끝나면 놓는다. */
	FBGPresentationTrackHold UIHold;

	FTimerHandle CardExitTimeoutTimer;
	FTimerHandle ResultInTimer;
	FTimerHandle ResultInFinishTimer;
	FTimerHandle OutroTimer;

	/** 지난 판정에 재생한 Reveal. 다음 시작 때 되감아 원래대로 돌린다. */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> LastRevealAnimation;

	/** 지난 판정에 재생한 결과창 등장. 다음 시작 때 되감는다. */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> LastResultInAnimation;
};

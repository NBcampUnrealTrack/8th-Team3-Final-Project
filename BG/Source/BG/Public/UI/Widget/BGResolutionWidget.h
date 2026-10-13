// BANG! 판정 결과 표시 위젯 (GDD §10 — "대실패·실패·성공·대성공을 색상, 아이콘, 텍스트,
// 짧은 연출로 구분한다").
//
// 카드를 낼 때마다 서버가 판정한 결과가 전원에게 도착하고, 이 위젯이 잠깐 띄웠다가 사라진다.
// GDD §9.2 "자동 판정은 짧은 연출과 로그를 제공하되 별도의 확인 입력을 요구하지 않는다" 에 따라
// 확인 버튼을 두지 않는다 — 뜨고 스스로 사라진다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGResolutionView.h"
#include "BGResolutionWidget.generated.h"

class UTextBlock;
class UBorder;

//	HUD(이 위젯을 품고 있는 쪽)가 구독하는 통지. 판정 표시의 시작·끝에 맞춰
//	평상시 패널 ↔ 연출 패널을 바꾸는 데 쓴다.
//
//	⚠️ 이 위젯이 HUD 를 직접 부르지 않는 이유: 참조 방향이 뒤집히면 판정 위젯이 HUD 의
//	   구성(어떤 패널이 있는지)까지 알아야 한다. 통지만 쏘고 무엇을 할지는 구독자가 정한다.
//	   위젯 참조를 PlayerController 만 소유하는 이 프로젝트의 규칙과 같은 이유다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGResolutionShownEvent, const FBGResolutionView&, Shown);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGResolutionHiddenEvent);

/**
 * WBP 서브클래스에 아래 이름의 위젯이 있으면 자동으로 채워진다. **전부 선택**이다 —
 * 하나도 없어도 컴파일과 실행이 유지되고, 있는 것만 갱신된다.
 *
 *   Text_Outcome  (TextBlock) — "대성공"
 *   Text_Roll     (TextBlock) — "눈 70 / 성공 65%"  (강제 판정이면 "강제")
 *   Text_Card     (TextBlock) — "뱅!  · 좌석 2"
 *   Text_Chance   (TextBlock) — "대실패 5 · 실패 20 · 성공 65 · 대성공 10"
 *   Border_Root   (Border)    — 결과 색으로 물들일 배경
 *
 * ── 게임 로직과의 연결 지점 ──
 *   들어오는 방향: ShowResolution() — PlayerController 가 판정 결과를 받아 밀어넣는다
 *   나가는 방향:   없다. 이 위젯은 읽기 전용 표시다.
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGResolutionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 판정 결과를 표시한다. DisplaySeconds 후 자동으로 숨긴다.
	 * 연달아 호출되면 이전 표시를 덮어쓰고 타이머를 다시 잡는다 — 카드를 빠르게 두 장 내도
	 * 첫 결과가 남아 두 번째를 가리지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Resolution")
	virtual void ShowResolution(const FBGResolutionView& InResolutionView);

	/** 즉시 숨긴다. 턴이 넘어가거나 판이 끝날 때 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Resolution")
	virtual void HideResolution();

	/**
	 * 사용한 카드가 퇴장을 시작했다. PlayerController 가 손패 신호를 받아 전달한다.
	 * 기본 위젯은 결과를 곧바로 띄우므로 무시한다 — 게이지처럼 카드 뒤에 이어 가는 위젯이 쓴다.
	 */
	virtual void NotifyCardExitStarted() {}

	UFUNCTION(BlueprintPure, Category = "BG|Resolution")
	const FBGResolutionView& GetResolutionView() const { return ResolutionView; }

	/** 결과 등급에 대응하는 색. 아이콘·연출을 BP 에서 만들 때도 이걸 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Resolution")
	FLinearColor GetTierColor(EBGResolutionTier Tier) const;

	/** 결과 등급의 표시 문구 ("대성공" 등). */
	UFUNCTION(BlueprintPure, Category = "BG|Resolution")
	static FText GetTierText(EBGResolutionTier Tier);

	/**
	 * 색·텍스트를 C++ 이 모두 채운 **뒤에** 불린다. 애니메이션·사운드·아이콘 교체처럼
	 * 데이터가 아니라 연출에 해당하는 것은 BP 에서 여기에 붙인다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Resolution")
	void OnResolutionShown(const FBGResolutionView& Shown);

	/** 자동으로 숨겨지거나 HideResolution 이 불렸을 때. 퇴장 연출용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Resolution")
	void OnResolutionHidden();

	/**
	 * 위 두 훅과 **같은 순간**을 위젯 바깥에 알린다.
	 *
	 * OnResolutionShown/Hidden 은 BlueprintImplementableEvent 라 이 위젯의 WBP 서브클래스에서만
	 * 구현할 수 있다. HUD 처럼 밖에 있는 구독자는 쓸 수 없어서 통지를 따로 둔다 —
	 * HUD 는 Event Construct 에서 여기에 바인딩해 패널을 바꾼다.
	 *
	 * ⚠️ Hidden 은 실제로 떠 있던 판정이 사라질 때만 온다(bShowing 참고).
	 */
	UPROPERTY(BlueprintAssignable, Category = "BG|Resolution")
	FOnBGResolutionShownEvent OnResolutionShownEvent;

	UPROPERTY(BlueprintAssignable, Category = "BG|Resolution")
	FOnBGResolutionHiddenEvent OnResolutionHiddenEvent;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;

	// ── 결과별 색상 (GDD §10) ─────────────────────────────────────
	//  기본값은 서버 디버그 로그(BGCardExec / BGPlayerController)가 쓰던 색과 맞췄다.
	//  콘솔과 화면의 색이 다르면 같은 판정을 두 가지로 기억하게 된다.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution|Color")
	FLinearColor CriticalFailureColor = FLinearColor(0.85f, 0.15f, 0.15f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution|Color")
	FLinearColor FailureColor = FLinearColor(0.63f, 0.63f, 0.63f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution|Color")
	FLinearColor SuccessColor = FLinearColor(1.f, 0.92f, 0.23f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution|Color")
	FLinearColor CriticalSuccessColor = FLinearColor(1.f, 0.55f, 0.f, 1.f);

	/** 등급을 못 읽었을 때(None). 눈에 띄어야 원인을 찾는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution|Color")
	FLinearColor UnknownTierColor = FLinearColor(1.f, 0.f, 1.f, 1.f);

	/**
	 * 표시 유지 시간(초). 0 이하면 자동으로 숨기지 않는다 — 연출을 BP 애니메이션으로
	 * 직접 제어하고 싶을 때 0 으로 두고 HideResolution 을 손수 부른다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution", meta = (ClampMin = "0.0"))
	float DisplaySeconds = 2.5f;

	/** 표시할 내용. 디자이너에서 값을 넣으면 프리뷰에 그대로 반영된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution")
	FBGResolutionView ResolutionView;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Outcome;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Roll;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Card;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Chance;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Root;

	/** ResolutionView 를 텍스트·색에 반영한다. */
	void RefreshVisual();

	/** 화면에 올리고 등장 통지(BP 훅 → 바깥)를 보낸다. 값은 미리 채워 둘 것. */
	void BeginShowing();

	/** DisplaySeconds 뒤 자동 숨김을 (다시) 건다. 2배속이면 DisplaySeconds 도 같은 비율로 줄인다. */
	void StartAutoHideTimer();

	/**
	 * 사용자 연출 배속 배율. 2배속이면 PresentationDoubleSpeedScale, 아니면 1. GameState 가 없으면 1.
	 * 서버 판정 UI 대기(GetResolutionPresentationSeconds)가 배속만큼 줄어드는데 이 위젯의 고정 길이
	 * (착지 정지 · 칸 · 결과창 · 표시 유지)가 그대로면 서버가 먼저 월드 연출을 터뜨린다.
	 */
	float GetUserSpeedScale() const;

	/**
	 * 걸려 있던 자동 숨김을 취소한다.
	 * ⚠️ ShowResolution 을 재정의해 표시를 늦추는 자식은 새 판정을 받을 때 반드시 부를 것 —
	 *    안 부르면 앞 판정의 타이머가 뒤 판정 연출 도중에 발동해 화면을 통째로 끈다.
	 */
	void ClearAutoHideTimer();

	bool IsShowing() const { return bShowing; }

private:

	/**
	 * 지금 판정이 떠 있는가. 퇴장 통지를 한 번만 쏘기 위한 것이다.
	 *
	 * ⚠️ 없으면 HUD 가 게임 시작과 동시에 복귀 연출을 한 번 돌린다 —
	 *    ABGPlayerController::SetResolutionWidget 이 등록 직후 빈 패널을 감추려고
	 *    HideResolution 을 부르기 때문이다(보여 준 적이 없는데 "사라졌다" 가 나간다).
	 */
	bool bShowing = false;

	/** 자동 숨김 타이머. 위젯이 파괴될 때 반드시 지운다. */
	FTimerHandle HideTimerHandle;
};

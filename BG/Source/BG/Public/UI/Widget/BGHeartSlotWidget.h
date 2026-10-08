// 하트 한 칸. 턴 패널(UBGTurnPanelWidget)이 여섯 개를 배치해 쓴다.
//
// 코드는 "이 칸이 찼는가 / 방금 꺼졌는가" 만 알린다. 꺼지는 모양은 WBP 의 Anim_Lose 가 정한다 —
// 연출을 바꾸려면 이 위젯의 WBP 하나만 고치면 이름표와 턴 패널에 함께 반영된다.
//
// WBP 에 둘 것:
//   Img_Heart  (Image, 필수)       — 하트 그림. 찬/빈 틴트가 여기 칠해진다.
//   Anim_Lose  (Widget Animation, 선택) — 칸이 꺼질 때. 끝나면 코드가 빈 칸 색으로 맞춘다.
//   Anim_Gain  (Widget Animation, 선택) — 칸이 다시 찰 때.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BGHeartSlotWidget.generated.h"

class UImage;
class UWidgetAnimation;

/** 하트 줄 하나가 직전에 보여준 값. 줄어든 칸만 연출하려면 직전 값을 알아야 한다. */
struct FBGHeartRowState
{
	//	INDEX_NONE 이면 아직 그린 적이 없다 — 첫 값으로는 연출하지 않는다.
	int32 LastHealth = INDEX_NONE;
	int32 LastMaxHealth = INDEX_NONE;
};

UCLASS(Abstract, Blueprintable)
class BG_API UBGHeartSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 찬 칸인지 정한다. bAnimate 면 바뀌는 방향에 맞춰 Anim_Lose / Anim_Gain 을 재생한다. */
	void SetFilled(bool bInFilled, bool bAnimate);

	UFUNCTION(BlueprintPure, Category = "BG|Heart")
	bool IsFilled() const { return bFilled; }

	/**
	 * 하트 줄 하나를 체력에 맞춘다. 두 위젯이 같은 규칙을 쓰도록 여기 둔다.
	 * 최대 체력을 넘는 칸은 자리까지 접고, 직전보다 줄었을 때만 꺼지는 칸을 연출한다.
	 * State 는 부르는 쪽이 줄마다 하나씩 들고 있는다.
	 */
	static void RefreshRow(TConstArrayView<UBGHeartSlotWidget*> Slots, int32 Health, int32 MaxHealth,
		FBGHeartRowState& State);

protected:
	virtual void NativeConstruct() override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 칸이 꺼졌다. 사운드·파티클처럼 애니메이션 트랙 밖의 것을 붙일 때 쓴다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Heart")
	void OnHeartLost();

	/** 칸이 다시 찼다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Heart")
	void OnHeartGained();

	//	⚠️ 선택이 아니라 필수다. 이름이 어긋나면 애니메이션(트랙이 이미지에 직접 묶인다)은 멀쩡히 나오는데
	//	   틴트만 조용히 안 칠해져 원인을 찾기 어렵다 — WBP 컴파일 에러로 바로 드러나게 한다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_Heart;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Lose;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Gain;

	/** 남아 있는 체력 하트의 색. */
	UPROPERTY(EditAnywhere, Category = "BG|Heart")
	FLinearColor FullTint = FLinearColor::White;

	/** 잃은 체력 하트의 색 — 자리는 남기고 어둡게만 만든다. */
	UPROPERTY(EditAnywhere, Category = "BG|Heart")
	FLinearColor EmptyTint = FLinearColor(0.06f, 0.06f, 0.06f, 0.55f);

private:
	void ApplyTint();

	bool bFilled = true;

	//	Anim_Lose 가 끝난 다음 틱에 틴트를 한 번 더 칠한다(OnAnimationFinished 주석 참고).
	bool bReapplyTintNextTick = false;
};

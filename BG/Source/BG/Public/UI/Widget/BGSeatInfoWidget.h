// BANG! 머리 위 이름표 — 캐릭터 위에 뜨는 좌석 정보.
//
// C++ 는 값만 넘기고 어떻게 그릴지는 WBP 가 정한다. 위젯 이름을 코드가 모르므로
// 배치를 바꿔도 코드는 손댈 일이 없다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGSeatView.h"
#include "BGSeatInfoWidget.generated.h"

/**
 * WBP 에서 OnSeatInfoChanged 를 구현해 자기 텍스트를 채운다.
 *
 * 이름은 항상 보여주고, 스탯은 ShouldShowStats() 가 참일 때만 보여줄 것.
 * 판이 시작되기 전에도 어트리뷰트는 이미 있어서 체력 값만으로는 구분되지 않는다.
 */
UCLASS(Blueprintable)
class BG_API UBGSeatInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 좌석 정보를 밀어넣는다. 매 프레임 불러도 안전하다. */
	UFUNCTION(BlueprintCallable, Category = "BG|SeatInfo")
	void SetSeatInfo(const FBGSeatView& InView);

	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	const FBGSeatView& GetSeatView() const { return View; }

	/** 체력 같은 판 중 정보를 보여줄 때인가. 판정 규칙을 BP 가 다시 짜지 않게 한다. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	bool ShouldShowStats() const { return View.bMatchRunning && View.MaxHealth > 0; }

	// 그래프에서 구조체를 쪼개지 않도록 바로 쓸 수 있는 형태로 준다.

	/** 사망·이탈 표시까지 붙인 이름. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	FText GetNameText() const;

	/** "4 / 5". 판이 시작되기 전에는 빈 텍스트. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	FText GetHealthText() const;

	/** 지금 차례인 사람을 다른 색으로 칠할 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	bool IsCurrentTurn() const { return View.bIsCurrentTurn; }

protected:
	//	WBP 가 같은 이름으로 만들어 두면 자동으로 연결된다. 없어도 동작한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_Health;

	/** 값이 들어왔다. 색·애니메이션처럼 텍스트 말고 더 할 것이 있으면 여기서 한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|SeatInfo")
	void OnSeatInfoChanged(const FBGSeatView& InView);

	// WBP 가 배치를 주지 않았을 때만 최소 배치를 만든다. WBP 를 만들면 이 경로는 죽는다.
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildFallbackLayout();

	UPROPERTY()
	FBGSeatView View;
};

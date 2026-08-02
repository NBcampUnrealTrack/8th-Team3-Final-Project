// BANG! 좌석 보드 — 모든 플레이어의 공개 정보를 모아 표시하고, 대상 선택 흐름을 관리한다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGSeatView.h"
#include "BGSeatBoardWidget.generated.h"

class UBGSeatWidget;
class UPanelWidget;
class UTextBlock;
class UWidget;

/**
 * WBP 서브클래스에 아래가 있어야 한다:
 *   Panel_Seats     (HorizontalBox 등 PanelWidget, 필수)
 *   Text_Prompt     (TextBlock, 선택 — 대상 선택 안내문)
 *   Panel_Prompt    (아무 Widget,  선택 — 선택 모드에서만 보이는 영역)
 * 그리고 디테일 패널에서 SeatWidgetClass 에 좌석 WBP 를 지정해야 한다.
 *
 * ── 게임 로직과의 연결 지점 ──
 *   들어오는 방향: SetSeats()             — 로직이 좌석 정보를 밀어넣는다 (주기적 호출 안전)
 *                 BeginTargetSelection() — 대상 선택 시작
 *   나가는 방향:   OnSeatSelected / OnTargetSelectionCancelled
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGSeatBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 좌석 정보를 갱신한다.
	 * 좌석 번호가 같으면 위젯을 재사용해 값만 바꾸므로, 매 프레임/타이머로 불러도
	 * 마우스 호버나 선택 상태가 초기화되지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Seat")
	void SetSeats(const TArray<FBGSeatView>& InSeats);

	UFUNCTION(BlueprintCallable, Category = "BG|Seat")
	void ClearSeats();

	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	UBGSeatWidget* FindSeatWidget(int32 SeatIndex) const;

	// ── 대상 선택 ──

	/** 대상 선택을 시작한다. 이미 진행 중이면 이전 요청은 취소 통지 후 대체된다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void BeginTargetSelection(const FBGTargetRequest& Request);

	/** 진행 중인 선택을 취소한다(OnTargetSelectionCancelled 발생). */
	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void CancelTargetSelection();

	UFUNCTION(BlueprintPure, Category = "BG|Target")
	bool IsSelectingTarget() const { return bSelectingTarget; }

	UFUNCTION(BlueprintPure, Category = "BG|Target")
	const FBGTargetRequest& GetActiveRequest() const { return ActiveRequest; }

	/** 좌석 위젯이 클릭됐을 때 호출한다. 선택 모드가 아니면 무시된다. */
	void NotifySeatClicked(int32 SeatIndex);

	UPROPERTY(BlueprintAssignable, Category = "BG|Target")
	FOnBGSeatSelected OnSeatSelected;

	UPROPERTY(BlueprintAssignable, Category = "BG|Target")
	FOnBGTargetSelectionCancelled OnTargetSelectionCancelled;

	/** 로직 없이 UI 만 테스트하기 위한 더미 좌석. */
	UFUNCTION(BlueprintCallable, Category = "BG|Seat|Debug")
	void FillWithDebugSeats(int32 Count = 4);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_Seats;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Prompt;

	/** 선택 모드에서만 보이는 영역. 없으면 Text_Prompt 만으로 안내한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Prompt;

	UPROPERTY(EditAnywhere, Category = "BG|Seat")
	TSubclassOf<UBGSeatWidget> SeatWidgetClass;

	UPROPERTY(EditAnywhere, Category = "BG|Seat")
	FMargin SeatPadding = FMargin(6.f, 0.f);

private:
	/** 선택 가능 여부를 각 좌석 위젯에 반영한다. */
	void ApplySelectionState();

	/** 선택 모드 안내 UI 표시/숨김. */
	void UpdatePromptVisibility();

	UPROPERTY()
	TMap<int32, TObjectPtr<UBGSeatWidget>> SeatWidgets;

	UPROPERTY()
	FBGTargetRequest ActiveRequest;

	bool bSelectingTarget = false;
};

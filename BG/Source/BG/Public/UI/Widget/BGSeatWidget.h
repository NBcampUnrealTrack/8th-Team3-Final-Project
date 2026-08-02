// BANG! 좌석 1개 위젯. 다른 플레이어의 공개 정보를 표시하고, 대상 선택 모드에서 클릭을 받는다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGSeatView.h"
#include "BGSeatWidget.generated.h"

class UTextBlock;
class UBorder;
class UBGSeatBoardWidget;

/**
 * WBP 서브클래스에 아래 이름의 위젯이 있어야 한다:
 *   Border_Root        (Border,    필수 — 상태 색 표시에 쓴다)
 *   Text_PlayerName    (TextBlock, 필수)
 *   Text_Health        (TextBlock, 필수 — "3 / 4")
 *   Text_HandCount     (TextBlock, 선택 — 손패 장수)
 *   Text_Distance      (TextBlock, 선택 — 거리, 사거리 디버깅에 매우 유용)
 *   Text_Role          (TextBlock, 선택 — 공개된 역할만)
 *   Text_Equipment     (TextBlock, 선택 — 장착 파란 카드 목록)
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGSeatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BG|Seat")
	void SetSeatView(const FBGSeatView& InSeatView);

	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	const FBGSeatView& GetSeatView() const { return SeatView; }

	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	int32 GetSeatIndex() const { return SeatView.SeatIndex; }

	/** 대상 선택 모드에서 이 좌석을 고를 수 있는지. 보드가 설정한다. */
	void SetSelectable(bool bInSelectable);

	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	bool IsSelectable() const { return bSelectable; }

	/** 소유 보드. 클릭을 여기로 되돌린다. 정의는 .cpp 에 있다(전방 선언 타입이라). */
	void SetOwningBoard(UBGSeatBoardWidget* InBoard);

protected:
	virtual void NativePreConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat", meta = (ExposeOnSpawn = true))
	FBGSeatView SeatView;

	// ── 상태 색 ──
	UPROPERTY(EditAnywhere, Category = "BG|Seat|Style")
	FLinearColor NormalColor = FLinearColor(1.f, 1.f, 1.f, 0.10f);

	/** 화면을 보는 본인의 좌석. */
	UPROPERTY(EditAnywhere, Category = "BG|Seat|Style")
	FLinearColor LocalPlayerColor = FLinearColor(0.25f, 0.55f, 1.f, 0.25f);

	/** 현재 턴인 좌석. */
	UPROPERTY(EditAnywhere, Category = "BG|Seat|Style")
	FLinearColor CurrentTurnColor = FLinearColor(1.f, 0.85f, 0.2f, 0.30f);

	UPROPERTY(EditAnywhere, Category = "BG|Seat|Style")
	FLinearColor DeadColor = FLinearColor(0.f, 0.f, 0.f, 0.45f);

	/** 대상 선택 모드에서 고를 수 있는 좌석. */
	UPROPERTY(EditAnywhere, Category = "BG|Seat|Style")
	FLinearColor SelectableColor = FLinearColor(0.2f, 0.9f, 0.3f, 0.30f);

	UPROPERTY(EditAnywhere, Category = "BG|Seat|Style")
	FLinearColor SelectableHoverColor = FLinearColor(0.3f, 1.f, 0.4f, 0.55f);

	/** 대상 선택 모드인데 고를 수 없는 좌석(사거리 밖 등). */
	UPROPERTY(EditAnywhere, Category = "BG|Seat|Style")
	FLinearColor NotSelectableColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.55f);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Border_Root;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PlayerName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Health;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HandCount;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Distance;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Role;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Equipment;

private:
	void RefreshVisual();

	UPROPERTY()
	TWeakObjectPtr<UBGSeatBoardWidget> OwningBoard;

	/** 대상 선택 모드가 아닐 때는 false. 즉 평소에는 클릭이 무시된다. */
	bool bSelectable = false;
	bool bHovered = false;
};

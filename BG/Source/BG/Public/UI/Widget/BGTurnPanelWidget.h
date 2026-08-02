// BANG! 턴 패널 — 현재 턴 상태를 보여주고 "턴 종료" 버튼을 제공한다.
// 이게 있으면 콘솔에 BG_EndTurn 을 치지 않고 플레이할 수 있다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGTurnView.h"
#include "BGTurnPanelWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * WBP 서브클래스에 아래 이름의 위젯이 있어야 한다:
 *   Button_EndTurn (Button,    필수)
 *   Text_Status    (TextBlock, 선택 — "내 차례" 등 안내문)
 *   Text_HandLimit (TextBlock, 선택 — "손패 5 / 한도 4")
 *   Text_Deck      (TextBlock, 선택 — 남은 덱 장수)
 *   Text_CardUse   (TextBlock, 선택 — "카드 1 / 2")
 *
 * ── 게임 로직과의 연결 지점 ──
 *   들어오는 방향: SetTurnView()      — PlayerController 가 주기적으로 밀어넣는다
 *   나가는 방향:   OnEndTurnRequested — PlayerController 가 구독해 서버로 보낸다
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGTurnPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BG|Turn")
	void SetTurnView(const FBGTurnView& InTurnView);

	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	const FBGTurnView& GetTurnView() const { return TurnView; }

	/** 턴 종료 버튼이 눌렸다. PlayerController 가 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "BG|Turn")
	FOnBGEndTurnRequested OnEndTurnRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	UFUNCTION()
	void HandleEndTurnClicked();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn", meta = (ExposeOnSpawn = true))
	FBGTurnView TurnView;

	/**
	 * 내 차례가 아닐 때 패널을 통째로 숨길지.
	 * false 면 남의 차례에도 상태 표시는 남고 버튼만 비활성화된다(진행 상황을 보기엔 이쪽이 낫다).
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Turn")
	bool bHideWhenNotMyTurn = false;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_EndTurn;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HandLimit;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Deck;

	/** "카드 1 / 2" — GDD §10 이 요구하는 사용한 카드 수 / 한도 표시. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CardUse;

private:
	void RefreshVisual();
};

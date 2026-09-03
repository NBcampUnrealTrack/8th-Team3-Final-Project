// BANG! 턴 패널 — 현재 턴 상태를 보여주고 "턴 종료" 버튼을 제공한다.
// 이게 있으면 콘솔에 BG_EndTurn 을 치지 않고 플레이할 수 있다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGSeatView.h"
#include "UI/View/BGTurnView.h"
#include "BGTurnPanelWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

/**
 * WBP 서브클래스에 아래 이름의 위젯이 있어야 한다:
 *   Button_EndTurn (Button,    필수)
 *   Text_Status    (TextBlock, 선택 — "내 차례" 등 안내문)
 *   Text_HandLimit (TextBlock, 선택 — "5 / 4". 손패 장수 / 한도)
 *   Text_Deck      (TextBlock, 선택 — 남은 덱 장수)
 *   Text_CardUse   (TextBlock, 선택 — "카드 1 / 2")
 *   Text_Rope      (TextBlock, 선택 — "밧줄 37초" 턴 제한 시간)
 *
 * 좌하단 본인 정보도 이 패널이 맡는다. 머리 위 이름표는 남만 띄우기 때문이다:
 *   Text_Name      (TextBlock, 선택 — 내 이름)
 *   Text_Job       (TextBlock, 선택 — 캐릭터 이름)
 *   Text_Health    (TextBlock, 선택 — "체력 5 / 5")
 *   Img_Heart_1..6 (Image,     선택 — 체력을 하트로. Text_Health 대신 쓰는 표시다)
 *   Text_Role      (TextBlock, 선택 — 내 역할. 본인 것이라 숨기지 않는다)
 *   Text_Equipment (TextBlock, 선택 — 장비 목록)
 *   손패 장수는 Text_HandLimit 이 이미 보여주므로 따로 두지 않는다.
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

	/** 좌하단에 띄울 내 좌석 정보. BGSeatInfoDisplayComponent 가 밀어넣는다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Turn")
	void SetLocalSeatInfo(const FBGSeatView& InView);

	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	const FBGSeatView& GetLocalSeatView() const { return LocalSeatView; }

	/** 체력·장비처럼 판 중에만 뜻이 있는 값을 보여줄 때인가. */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	bool ShouldShowLocalStats() const { return LocalSeatView.bMatchRunning && LocalSeatView.MaxHealth > 0; }

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

	/** 밧줄(턴 제한 시간). 남은 초는 매 틱 줄어들므로 델리게이트가 아니라 여기서 직접 읽는다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Rope;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Job;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Health;

	/**
	 * 최대 체력 여섯 칸을 미리 두고 남는 것은 접는다.
	 * 하나도 없으면 Text_Health 만으로 표시된다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Heart_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Heart_2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Heart_3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Heart_4;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Heart_5;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Heart_6;

	/** 남아 있는 체력 하트의 색. */
	UPROPERTY(EditAnywhere, Category = "BG|Turn")
	FLinearColor HeartFullTint = FLinearColor::White;

	/** 잃은 체력 하트의 색 — 자리는 남기고 어둡게만 만든다. */
	UPROPERTY(EditAnywhere, Category = "BG|Turn")
	FLinearColor HeartEmptyTint = FLinearColor(0.06f, 0.06f, 0.06f, 0.55f);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Role;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Equipment;

	UPROPERTY()
	FBGSeatView LocalSeatView;

	/** 값이 들어왔다. 초상화·하트 아이콘처럼 텍스트 말고 더 할 것이 있으면 여기서 한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Turn")
	void OnLocalSeatInfoChanged(const FBGSeatView& InView);

	//	밧줄이 붙고 꺼지는 순간만 델리게이트로 받는다. 남은 초는 NativeTick 이 읽는다.
	UFUNCTION()
	void HandleRopeChanged(int32 Seat, float SecondsRemaining, float TotalSeconds);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void RefreshVisual();

	//	LocalSeatView 의 체력을 하트 줄에 반영한다.
	void RefreshHealthHearts();

	// 밧줄 컴포넌트를 찾아 구독한다. 아직 없으면 false — 다음 틱에 다시 시도한다.
	bool TryBindRope();

	void RefreshRopeText();

	TWeakObjectPtr<class UBGRopeComponent> Rope;
};

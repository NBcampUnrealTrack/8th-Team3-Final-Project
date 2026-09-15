// PvE 보상 선택 - 후보 카드(최대 3장) 중 하나를 클릭해서 고르는 위젯.
// UBGPvELobbyWidget과 짝이다 - 보상 후보가 있을 때만 뜨고, 선택하거나 판이 시작되면
// 스스로 사라진다.
//
// WBP 에 카드 슬롯 3개를 CardButton0~2(Button) 안에 CardWidget0~2(UBGCardWidget)를
// 자식으로 넣어 둘 것. 카드 위젯은 드래그 전용이라 Button 으로 감싸고 SetDragLocked(true)로
// 드래그를 꺼서 클릭이 Button 까지 그대로 전달되게 한다 - 그래프 노드는 필요 없다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BGRewardSelectWidget.generated.h"

class UButton;
class UBGCardWidget;
class ABGGameState;

UCLASS(Abstract, Blueprintable)
class BG_API UBGRewardSelectWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CardButton0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBGCardWidget> CardWidget0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CardButton1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBGCardWidget> CardWidget1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CardButton2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBGCardWidget> CardWidget2;

private:
	UFUNCTION()
	void HandleCard0Clicked();
	UFUNCTION()
	void HandleCard1Clicked();
	UFUNCTION()
	void HandleCard2Clicked();

	void SelectSlot(int32 SlotIndex);

	/** 후보를 슬롯에 채워 보여준다. 후보가 3보다 적으면 남는 슬롯은 숨긴다. */
	void RefreshCandidates();

	/** GameState 는 클라에 늦게 올 수 있어 재시도한다(UBGSessionMenuWidget::TryBindPhaseDelegate 와 같은 이유). */
	void TryBindTurnStateDelegate();

	UFUNCTION()
	void HandleTurnStateChanged();

	TWeakObjectPtr<ABGGameState> BoundGameState;

	/** 슬롯 순서대로 채운 후보 CardId - 버튼 클릭 시 어떤 카드인지 알아야 한다. */
	TArray<FName> SlotCardIds;
};

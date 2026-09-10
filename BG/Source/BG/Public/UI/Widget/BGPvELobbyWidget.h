// PvE 로비 - "게임 시작" 버튼 하나뿐인 위젯. 세션이 없는 PvE 전용 진입점이다
// (PvP 는 UBGSessionMenuWidget 의 세션 로비를 쓴다). 판이 시작되면(OnTurnStateChanged)
// 스스로 화면에서 사라진다.
//
// WBP 에 StartButton 이름으로 Button 을 올려둘 것 - 그래프 노드는 필요 없다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BGPvELobbyWidget.generated.h"

class UButton;
class ABGGameState;

UCLASS(Abstract, Blueprintable)
class BG_API UBGPvELobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartButton;

private:
	UFUNCTION()
	void HandleStartClicked();

	/** GameState 는 클라에 늦게 올 수 있어 재시도한다(UBGSessionMenuWidget::TryBindPhaseDelegate 와 같은 이유). */
	void TryBindTurnStateDelegate();

	UFUNCTION()
	void HandleTurnStateChanged();

	TWeakObjectPtr<ABGGameState> BoundGameState;
};

// 튜토리얼 설명 팝업 - 제목과 본문, 다음 버튼.
// 서버의 UBGTutorialComponent 가 정한 활성 단계(GameState::GetActiveTutorialStep)를 스스로 보고
// 단계 표(UBGGameInstance::GetTutorialStepTable)에서 문구를 읽어 보여준다. 단계가 없으면 접힌다.
// 다음 버튼은 읽기형(NextButton) 단계에서만 보이고, 누르면 서버에 요청만 보낸다.
//
// WBP_BGHUD 안에 얹어 둘 것(UBGRewardSelectWidget 과 같은 관례). 떼어내지 않고 접는다.
// WBP 에 Text_Title, Text_Body (TextBlock), Btn_Next (Button) 를 같은 이름으로 배치해야 한다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BGTutorialPopupWidget.generated.h"

class UButton;
class UTextBlock;
class ABGGameState;

UCLASS(Abstract, Blueprintable)
class BG_API UBGTutorialPopupWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Body;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Next;

private:
	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleTurnStateChanged();

	/** GameState 는 클라에 늦게 올 수 있어 통지를 받을 때마다 다시 붙는다(UBGRewardSelectWidget 과 같은 이유). */
	void TryBindTurnStateDelegate();

	TWeakObjectPtr<ABGGameState> BoundGameState;

	/** 지금 화면에 올라 있는 단계. 같은 단계의 재통지에는 문구를 다시 채우지 않는다. */
	FName ShownStep;
};

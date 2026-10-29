// 튜토리얼 진행기 - 서버 전용. 단계 표(FBGTutorialStepRow)를 순서대로 읽어 한 단계씩 활성화한다.
//
// 활성 단계는 GameState 에 복제되고(SetActiveTutorialStep), 클라이언트의 팝업 위젯이 그것을 본다.
// 읽기형(NextButton) 단계는 활성화하는 순간 GameState 의 튜토리얼 정지를 켜고, 다음 버튼이 오면 푼다.
// 행동형(CardPlayed / TurnEnded) 단계는 정지 없이 안내만 띄우고 그 행동이 일어나면 끝낸다.
//
// 카드 연출(판정 UI, 월드 연출, 시네마틱) 중에는 다음 단계를 활성화하지 않는다.
// 연출이 끝날 때마다 턴 상태 통지가 나가므로 그 통지에서 다시 확인한다.
// 튜토리얼 판이 아니면 아무것도 하지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BGTutorialComponent.generated.h"

class ABGGameState;
class UDataTable;
struct FBGTutorialStepRow;

UCLASS(ClassGroup = (BG))
class BG_API UBGTutorialComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGTutorialComponent();

	// 팝업의 다음 버튼이 PlayerController 를 거쳐 온다. 지금 단계가 읽기형일 때만 받아 준다.
	void HandleNextPressed();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	// GameState::OnTurnStateChanged 구독 콜백. 턴, 페이즈, 연출 상태가 바뀔 때마다 온다.
	UFUNCTION()
	void HandleTurnStateChanged();

	// 지금 할 일이 있는지 보고 처리한다. 처리 중에 다시 불리면 끝난 뒤 한 번 더 돈다.
	void Evaluate();
	void EvaluateOnce();

	bool IsTriggerMet(const FBGTutorialStepRow& Row) const;
	bool IsCompleteMet(const FBGTutorialStepRow& Row) const;

	void ActivateStep(int32 Index, const FBGTutorialStepRow& Row);
	void CompleteStep();

	// 사람(AI 가 아닌) 플레이어의 좌석. 없으면 INDEX_NONE.
	int32 FindHumanSeat() const;

	ABGGameState* GetBGGameState() const;
	const UDataTable* GetStepTable() const;

	// Order 순으로 정렬한 Row 이름. BeginPlay 에서 한 번 만든다.
	TArray<FName> StepIds;

	// 다음에 활성화할 단계.
	int32 NextIndex = 0;

	// 지금 활성인 단계. 없으면 INDEX_NONE.
	int32 ActiveIndex = INDEX_NONE;

	// 행동형 단계의 완료 판정 기준. 활성화한 순간의 값이다.
	int32 BaselineCardsUsed = 0;
	int32 BaselineSeat = INDEX_NONE;

	// Evaluate 안에서 GameState 를 바꾸면 통지가 다시 들어온다. 그 재진입을 막는다.
	bool bEvaluating = false;
	bool bReevaluate = false;
};

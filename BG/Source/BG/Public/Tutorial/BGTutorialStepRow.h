// 튜토리얼 단계 표(DT_BGTutorialSteps)의 한 행.
//
// 한 행이 설명 팝업 하나다. Row 이름은 아무거나 상관없다(Step_01 ...) - 진행 순서는 Order 로 정한다.
// 서버의 UBGTutorialComponent 가 순서대로 한 단계씩 활성화하고, 클라이언트의 UBGTutorialPopupWidget 이
// 같은 표에서 제목과 본문을 읽어 보여준다. 표는 UBGGameInstance 의 TutorialStepTable 한 곳에서 지정한다.
//
// 시작 조건은 "그 상태가 참인가"로 본다(그 순간이 오기를 기다리는 게 아니다).
// 예를 들어 MyTurnStart 단계는 이전 단계가 끝난 시점에 이미 내 턴이면 바로 뜬다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BGTutorialStepRow.generated.h"

UENUM(BlueprintType)
enum class EBGTutorialTrigger : uint8
{
	AfterPrevious  UMETA(DisplayName = "이전 단계 직후"),
	MatchStart     UMETA(DisplayName = "판 시작 후 첫 턴"),
	MyTurnStart    UMETA(DisplayName = "내 턴 시작"),
	EnemyTurnStart UMETA(DisplayName = "적 턴 시작"),
	AfterMyCard    UMETA(DisplayName = "내가 카드를 낸 직후"),

	// 도입 연출 단계에 맞춘 시작 조건. 읽기형(NextButton)이면 팝업이 떠 있는 동안 그 단계의 타이머가 선다.
	// 그 단계가 이미 지났으면 이 행은 건너뛴다(BG_SkipIntro 로 넘겼을 때 뒤 단계가 막히지 않게).
	IntroRoleReveal UMETA(DisplayName = "도입: 내 역할 확인"),
	IntroHeroReveal UMETA(DisplayName = "도입: 용사 공개"),
	IntroFirstTurn  UMETA(DisplayName = "도입: 첫 턴 안내")
};

UENUM(BlueprintType)
enum class EBGTutorialComplete : uint8
{
	// 읽기형. 팝업이 떠 있는 동안 게임이 멈추고, 다음 버튼을 눌러야 이어진다.
	NextButton UMETA(DisplayName = "다음 버튼 (게임 정지)"),
	// 행동형. 게임은 멈추지 않고 안내만 띄운다. 내가 카드를 내면 끝난다.
	CardPlayed UMETA(DisplayName = "카드를 내면 (정지 없음)"),
	// 행동형. 내 턴이 끝나면 끝난다.
	TurnEnded  UMETA(DisplayName = "턴을 넘기면 (정지 없음)")
};

USTRUCT(BlueprintType)
struct FBGTutorialStepRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 진행 순서. 작은 값부터. 같으면 Row 이름 순서는 보장하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EBGTutorialTrigger Trigger = EBGTutorialTrigger::AfterPrevious;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Body;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EBGTutorialComplete CompleteOn = EBGTutorialComplete::NextButton;

	/** 화면에서 강조할 대상 이름. 아직 쓰이지 않는다(강조 연출은 이후 단계에서 붙인다). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName HighlightTarget;
};

// BANG! 판정 결과 UI 전용 뷰모델 (GDD §4.1 / §10).
//
// FBGCardView / FBGSeatView 와 같은 원칙 — UI 는 ABGGameState 도 ASC 도 DT 도 모른다.
// 게임 로직의 EBGDiceOutcome 을 여기로 들이지 않는 이유도 같다: Card/BGGameDataTypes.h 를
// include 하는 순간 뷰가 카드 데이터·GE·태그까지 끌고 오게 된다.
// 변환은 ABGPlayerController::HandleCardResolved 한 곳에서만 일어난다.

#pragma once

#include "CoreMinimal.h"
#include "UI/View/BGCardView.h"   // FBGCardChanceView — 같은 뷰 계층이라 의존해도 된다
#include "BGResolutionView.generated.h"

/**
 * 판정 4단계 (GDD §4.1). 게임 로직의 EBGDiceOutcome 을 UI 쪽에 복사한 것이다.
 *
 * ⚠️ EBGDiceOutcome 에 등급을 추가하면 여기도 함께 넣을 것.
 *    변환은 ABGPlayerController::ToResolutionTier 한 곳에만 있으므로 거기서 컴파일 에러가 난다.
 */
UENUM(BlueprintType)
enum class EBGResolutionTier : uint8
{
	None            UMETA(DisplayName = "없음"),
	CriticalFailure UMETA(DisplayName = "대실패"),
	Failure         UMETA(DisplayName = "실패"),
	Success         UMETA(DisplayName = "성공"),
	CriticalSuccess UMETA(DisplayName = "대성공")
};

/**
 * 판정 1회의 결과 — 화면에 그릴 것 전부.
 *
 * GDD §4.1 은 "모든 플레이어에게 동일한 결과를 표시" 를 요구한다. 그래서 여기 들어가는 값은
 * 전부 공개 정보다 — 남의 손패처럼 숨겨야 하는 것은 넣지 않는다.
 */
USTRUCT(BlueprintType)
struct FBGResolutionView
{
	GENERATED_BODY()

	/** 판정 결과. None 이면 아직 아무 판정도 없다(위젯은 숨긴 상태). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution")
	EBGResolutionTier Tier = EBGResolutionTier::None;

	/**
	 * 굴린 눈 (0~99). 판정이 강제된 경우엔 의미가 없다 — bForced 를 함께 볼 것.
	 *
	 * 굴림 값을 보여주는 이유는 GDD §11 "설정 확률 vs 실제 체감 비교" 때문이다.
	 * 눈과 확률을 같이 보여주면 플레이어가 "성공 65%인데 눈 70이라 실패" 를 스스로 납득한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution")
	int32 Roll = INDEX_NONE;

	/**
	 * 준비·함정(Status.NextCard.Force*)으로 굴림 결과가 덮어써졌는가.
	 *
	 * ⚠️ 이 값을 표시하지 않으면 "눈 3 인데 대성공" 이 화면에 그대로 나와 버그로 보인다.
	 *    강제된 판정은 눈을 숨기거나 "강제" 를 함께 적어야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution")
	bool bForced = false;

	/** 어떤 카드의 판정인가. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution")
	FText CardName;

	/** 누가 썼는가 — 좌석 번호. 모르면 INDEX_NONE. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution")
	int32 SeatIndex = INDEX_NONE;

	/** 누가 썼는가 — 표시용 이름. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution")
	FText SeatName;

	/** 이 판정에 실제로 쓰인 확률(%). 보정까지 반영된 최종값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Resolution")
	FBGCardChanceView Chance;

	bool IsSet() const { return Tier != EBGResolutionTier::None; }
};

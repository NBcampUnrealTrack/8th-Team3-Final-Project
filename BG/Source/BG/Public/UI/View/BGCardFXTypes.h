// 카드 연출 훅이 주고받는 맥락 타입 모음.
//
// ⚠️ 훅에 인자를 하나씩 늘어놓지 않고 구조체로 묶는 이유:
//    BlueprintImplementableEvent 는 시그니처가 바뀌면 그 이벤트를 쓰던 BP 그래프가
//    전부 끊긴다. 구조체 하나만 넘기면 나중에 필드를 추가해도 기존 그래프가 살아남는다.
//
// 이 타입들을 BGCardView.h 에 같이 두지 못하는 이유는 순환 의존이다 —
// 여기서 EBGResolutionTier 가 필요한데, 그것을 가진 BGResolutionView.h 가
// 거꾸로 BGCardView.h 를 include 하고 있다. 그래서 아래쪽(카드 뷰)이 아니라
// 위쪽(연출)에 별도 헤더를 둔다.

#pragma once

#include "CoreMinimal.h"
#include "UI/View/BGCardView.h"         // EBGCardFXArchetype
#include "UI/View/BGResolutionView.h"   // EBGResolutionTier
#include "BGCardFXTypes.generated.h"

/**
 * 카드가 손패에 들어온 이유. 연출의 출발점과 성격을 가른다.
 *
 * ⚠️ 지금은 서버가 "손패가 이렇게 바뀌었다" 만 보내고 사유는 보내지 않는다.
 *    그래서 실제로 채워지는 값은 Draw / InitialHand 뿐이고, 나머지는 자리만 잡아 둔 것이다.
 *    서버에 이동 사유 이벤트가 생기면 UBGHandWidget::SetHand 의 유도부만 고치면 된다.
 */
UENUM(BlueprintType)
enum class EBGCardGainReason : uint8
{
	Draw        UMETA(DisplayName = "덱에서 뽑음"),
	InitialHand UMETA(DisplayName = "판 시작 배분"),
	Steal       UMETA(DisplayName = "상대에게서 훔침"),
	Reward      UMETA(DisplayName = "보상")
};

/**
 * 연출이 터질 자리. 효과 종류에서 유도된다.
 *
 * 피해는 맞는 쪽에서, 회복은 자기 자리에서 터져야 한다 — 전부 카드 위치에서 터지면
 * 무엇이 누구에게 일어났는지 읽히지 않는다.
 */
UENUM(BlueprintType)
enum class EBGCardFXTargetKind : uint8
{
	Caster UMETA(DisplayName = "시전자"),
	Target UMETA(DisplayName = "대상"),
	Deck   UMETA(DisplayName = "덱")
};

/**
 * 연출 한 조각 — 프리미티브 하나의 재생 지시.
 *
 * 카드마다 연출을 만들지 않고 이 조각들의 **목록**으로 표현한다. 목록은 새로 적지 않고
 * DT_BGCard 의 OutcomeEffects 에서 유도한다 — 카드 효과가 곧 연출 구성이다.
 *
 *   속공(대성공) = [ Draw 1, Damage 1, Draw 1 ]  ← 뽑기 + 피해 + 한도회복
 *
 * ⚠️ 이 목록을 대표값 하나로 줄이면(EBGCardFXArchetype) 복합 효과가 뭉개진다.
 *    Archetype 은 "카드 테두리 색" 처럼 하나만 필요한 곳에만 쓸 것.
 */
USTRUCT(BlueprintType)
struct FBGCardFXStep
{
	GENERATED_BODY()

	/** 무슨 종류의 연출인가. 프리미티브를 고르는 키다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	EBGCardFXArchetype Op = EBGCardFXArchetype::None;

	/** 세기 — 피해량 · 장수. 연출 크기를 여기에 비례시키면 된다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	int32 Amount = 0;

	/** 어디서 터지는가. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	EBGCardFXTargetKind TargetKind = EBGCardFXTargetKind::Caster;
};

/** 카드가 손패에 나타나는 순간의 맥락. OnDealt 로 전달된다. */
USTRUCT(BlueprintType)
struct FBGCardDealContext
{
	GENERATED_BODY()

	/** 이번 묶음 안에서 몇 번째인가 (0부터). 손패에서의 위치가 아니다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	int32 OrderInBatch = 0;

	/** 이번 묶음의 총 장수. 마지막 장 판별은 OrderInBatch == TotalInBatch - 1. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	int32 TotalInBatch = 1;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	EBGCardGainReason Reason = EBGCardGainReason::Draw;

	/** 출발점(손패 패널 로컬 좌표). 잔상·궤적 연출이 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	FVector2D FromPosition = FVector2D::ZeroVector;
};

/**
 * 사용한 카드의 효과가 확정된 순간의 맥락. OnPlayResolved 로 전달된다.
 *
 * ⚠️ 장비(파란 카드)는 판정을 거치지 않는다. 그때는 Tier 가 None 이고 Roll 은
 *    INDEX_NONE 이다 — 등급으로 분기하는 연출은 None 을 반드시 처리할 것.
 */
USTRUCT(BlueprintType)
struct FBGCardPlayContext
{
	GENERATED_BODY()

	/** 서버가 발급한 판정 연출 번호. 클라이언트의 전체 연출 완료 통지에 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	int32 PresentationId = 0;

	/** 판정 등급. 장비처럼 판정하지 않는 카드는 None. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	EBGResolutionTier Tier = EBGResolutionTier::None;

	/** 굴린 눈 0~99. 판정이 없었거나 강제됐으면 의미가 없다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	int32 Roll = INDEX_NONE;

	/** 준비·함정으로 굴림 결과가 덮어써졌는가. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	bool bForced = false;

	/**
	 * 실제로 발동한 효과들 — **연출의 정식 입력**이다.
	 * BP 는 이 목록을 ForEach 로 돌면서 프리미티브를 재생한다.
	 *
	 * 판정 등급에 해당하는 효과만 들어온다. 실패했으면 비어 있을 수도 있다
	 * (GDD §4.4 — 대실패가 모든 카드에 불이익을 강제하지는 않는다).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	TArray<FBGCardFXStep> Steps;

	/**
	 * 이 카드의 대표 성격. 하나만 필요한 곳(테두리 색, 아이콘)에 쓴다.
	 * ⚠️ 연출 재생에는 쓰지 말 것 — 복합 효과가 뭉개진다. 그때는 Steps 를 본다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	EBGCardFXArchetype Archetype = EBGCardFXArchetype::None;

	/** 카드 대분류(장비/공격/마법). HUD 가 전투 연출을 켤지 고를 때 읽는다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	EBGCardCategory Category = EBGCardCategory::Magic;

	/** 카드를 낸 좌석. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	int32 SeatIndex = INDEX_NONE;

	/** 판정 없이 확정된 경우(장비 장착 등) true. Tier 를 믿을 수 없다는 뜻이다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	bool bResolvedWithoutRoll = false;

	/**
	 * 이 판정에 실제로 쓰인 확률(%). 보정까지 반영된 최종값이다.
	 * 눈금 경계는 UBGCardViewLibrary::GetRollBands 로 뽑는다.
	 *
	 * ⚠️ 클라에서 다시 계산하지 말 것. 보정은 시전자의 어트리뷰트에 달려 있고
	 *    남의 어트리뷰트는 복제되지 않는다 — 남의 판정만 확률이 틀리게 나온다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card|FX")
	FBGCardChanceView Chance;
};

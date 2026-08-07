// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BGCardView.generated.h"

class UTexture2D;

/**
 * UI 전용 카드 뷰모델.
 *
 * 게임 로직 쪽 카드 타입(FBGCardInstance / FBGCardRow)에 의존하지 않는다.
 * 변환은 UBGCardViewLibrary::MakeCardView 한 곳에서만 일어난다 — 그 함수가 로직/UI 경계다.
 */
/**
 * 4단계 판정 확률(%). GDD §4.2 의 정규화를 이미 거친 값이라 그대로 그리면 된다 — 합은 100 이다.
 *
 * 비율(FBGOutcomeWeights)이 아니라 확률(%)로 들고 있는 이유:
 * 기획이 입력하는 것은 "상대 비율"(합이 100 일 필요가 없다)이고 플레이어가 봐야 하는 것은
 * "실제 확률"이다. 그 변환을 UI 가 매번 하면 정규화 규칙이 두 곳에 생긴다.
 */
USTRUCT(BlueprintType)
struct FBGCardChanceView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Chance")
	float CriticalFailure = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Chance")
	float Failure = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Chance")
	float Success = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Chance")
	float CriticalSuccess = 0.f;

	/** 표시 공간이 좁을 때 쓰는 축약값 — "성공+대성공" (GDD §10). */
	float GetSuccessOrBetter() const { return Success + CriticalSuccess; }

	/** 판정 비율이 아예 없는 카드(장비 등)면 false. */
	bool IsSet() const { return (CriticalFailure + Failure + Success + CriticalSuccess) > 0.f; }
};

/**
 * 연출 원형(archetype) — "이 카드는 대충 어떤 종류의 일을 하는가".
 *
 * 카드마다 전용 연출을 만들면 카드 수만큼 에셋이 필요해 끝나지 않는다. 대신 카드가
 * 하는 일에서 원형을 뽑아 공용 연출에 매핑하고, 정말 특별한 몇 장만 전용으로 만든다.
 *
 * ⚠️ 이 값은 DT 에 따로 적지 않는다. FBGCardRow 의 OutcomeEffects / TypeTag 에서
 *    유도한다 — 두 곳에 적으면 카드 효과를 바꿨을 때 연출만 옛것으로 남는다.
 *    유도는 BGCardViewLibrary.cpp 의 DeriveFXArchetype 한 곳에서만 일어나고,
 *    결과는 FBGCardView::FXArchetype 에만 실린다. 다른 곳에서 다시 유도하지 말 것.
 */
UENUM(BlueprintType)
enum class EBGCardFXArchetype : uint8
{
	None    UMETA(DisplayName = "없음"),
	Damage  UMETA(DisplayName = "공격"),      // 대상에게 피해
	Heal    UMETA(DisplayName = "회복"),      // 자신 회복
	Draw    UMETA(DisplayName = "수급"),      // 카드 뽑기 · 사용한도 회복
	Steal   UMETA(DisplayName = "탈취"),      // 상대 손패를 가져옴
	Discard UMETA(DisplayName = "파괴"),      // 상대 손패를 버림
	Status  UMETA(DisplayName = "상태 부여"),  // "다음 1회" 상태
	Equip   UMETA(DisplayName = "장착")       // 파란 카드
};

USTRUCT(BlueprintType)
struct FBGCardView
{
	GENERATED_BODY()

	/** 게임 로직의 카드 인스턴스 ID. 서버에 "이 카드를 사용" 을 모호함 없이 지정하기 위한 키. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	int32 InstanceId = INDEX_NONE;

	/** 카드 종류 키 (데이터 테이블 Row 이름 등). 로직/디버깅용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	FName CardId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	FText Description;

	/**
	 * 대상 좌석을 지정해야 하는 카드인가 (BANG!, 당황, 캣 발루, 결투 등).
	 * UI는 이 값을 그대로 전달만 한다. 대상 지정 플로우는 이 작업 범위가 아니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	bool bNeedsTarget = false;

	/** 클라이언트 측 표시용 힌트일 뿐이다. 실제 사용 가능 판정은 항상 서버 권위. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	bool bPlayable = true;

	// ── 4단계 확률 (GDD §4.4 / §6.1 / §8 / §10) ───────────────────────
	//
	// 이 값들은 표시 전용이다. 서버는 자기 데이터로 다시 굴리므로(GDD §9.2)
	// 여기가 틀려도 판정은 흔들리지 않는다 — 대신 플레이어를 속이게 되므로 정확해야 한다.

	/** 판정하는 카드인가. 파란(장비) 카드는 false — UI 는 확률 영역을 통째로 숨긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	bool bHasChances = false;

	/** 카드 자체의 확률. DT_BGCard 의 OutcomeWeights 만 반영한 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	FBGCardChanceView BaseChance;

	/**
	 * 장비·캐릭터 보정까지 반영한 최종 확률 (GDD §8 "장비 효과가 적용된 최종 확률").
	 * 보정이 없으면 BaseChance 와 같은 값이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	FBGCardChanceView FinalChance;

	/** 보정으로 확률이 실제로 달라졌는가. UI 가 "65% → 78%" 처럼 변화를 드러낼 때 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	bool bChanceModified = false;

	/**
	 * 이 카드의 연출 원형. 카드 데이터에서 유도된 값이다.
	 * 전용 연출이 없는 카드는 이 값만 보고 공용 연출을 고르면 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	EBGCardFXArchetype FXArchetype = EBGCardFXArchetype::None;

	/**
	 * 테두리 안쪽에 그릴 그림. DT_BGCard 의 ArtTexture 를 로드한 결과다.
	 *
	 * ⚠️ null 일 수 있다 — DT 에 그림을 아직 안 넣은 카드가 그렇다. 위젯은 null 이면
	 *    폴백(무지 사각형·물음표 등)을 그릴 것. 그냥 Set Brush 하면 카드가 투명해져서
	 *    "카드가 안 보인다" 로 시간을 버린다.
	 *
	 * 테두리는 여기 없다 — 카드마다 다른 것은 안쪽 그림뿐이고, 테두리는 타입·등급으로
	 * 갈리므로 위젯이 TypeTag 계열 값에서 골라 겹쳐 그리는 편이 데이터가 덜 중복된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	TObjectPtr<UTexture2D> ArtTexture = nullptr;

	bool IsValid() const { return InstanceId != INDEX_NONE; }
};

/**
 * 카드 사용 요청이 발생했을 때 브로드캐스트된다.
 *
 * 권장 바인딩 지점은 UBGHandWidget::OnCardPlayRequested 한 곳이다.
 * 핸드와 플레이 존 양쪽에 모두 바인딩하면 같은 요청을 두 번 받게 된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGCardPlayRequested, const FBGCardView&, Card);

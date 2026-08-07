// BANG! 확률 판정 전담 컴포넌트 — ABGGameState 에 부착된다.
//
// 왜 GameState 인가 (GameMode 가 아니라):
//   · GameMode 는 클라에 존재하지 않는다. 판정 설정(면수/비율)을 클라가 못 읽으면
//     "성공 확률 62%" 같은 UI 를 만들 수 없다. 컴포넌트 기본값은 CDO 라 양쪽이 동일하다.
//   · 시드 난수(RandomStream)의 소유자가 GameState 다. 재현 가능한 판정을 하려면 옆에 있어야 한다.
//   · 같은 성격의 PerformDrawCheck 가 이미 GameState 에 있다 — 확률 판정을 한 곳에 모은다.
//
// ⚠️ 굴리기(RollDice/RollForOutcome/RollOutcome/RandomIndex)는 서버 전용이다.
//    설정값 읽기와 ApplyModifiers·ClassifyRoll(순수 계산)은 클라에서도 안전하다.
//
// ⚠️ 판정 경로가 둘이다. 정식은 **RollOutcome**(카드 비율 기반, GDD §4.2)이고,
//    RollDice/ClassifyRoll/RollForOutcome 은 구형 비활성 경로다 — 눈의 방향까지 반대다.
//    아래 "판정 (구형 경로)" 블록의 경고를 반드시 읽을 것.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "GameplayTagContainer.h"
#include "Card/BGGameDataTypes.h"   // EBGDiceOutcome / FBGOutcomeWeights
class UAbilitySystemComponent;

#include "BGDiceComponent.generated.h"

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGDiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGDiceComponent();

	/**
	 * 월드에서 판정 컴포넌트를 얻는다 (GameState 에 붙어 있다).
	 * 호출부가 GameState→FindComponentByClass 를 매번 쓰지 않도록 감싼 것.
	 * 클라에서도 유효하다(설정 읽기용). 없으면 nullptr.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Dice", meta = (WorldContext = "WorldContextObject"))
	static UBGDiceComponent* Get(const UObject* WorldContextObject);

	// ── 설정 (BP GameState 디폴트에서 튜닝) ──────────────────────
	// 주사위 면 수 (기본 24면체).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BG|Dice", meta = (ClampMin = "2"))
	int32 DiceFaces = 24;

	// [비활성] 각 단계의 "상대 비율". 합이 1 일 필요 없다(내부에서 정규화).
	// 낮은 눈=대실패 … 높은 눈=대성공. 예) 5/45/45/5 로 두면 극단이 5% 씩.
	//
	// ⚠️ ClassifyRoll 전용이다 — 구형 경로라 실제 판정에 쓰이지 않는다. 여기를 아무리 고쳐도
	//    게임의 확률은 바뀌지 않는다. 카드 확률은 DT_BGCard 의 OutcomeWeights 가 정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BG|Dice|Deprecated", meta = (ClampMin = "0.0"))
	float CriticalFailureRatio = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BG|Dice|Deprecated", meta = (ClampMin = "0.0"))
	float FailureRatio = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BG|Dice|Deprecated", meta = (ClampMin = "0.0"))
	float SuccessRatio = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BG|Dice|Deprecated", meta = (ClampMin = "0.0"))
	float CriticalSuccessRatio = 0.1f;

	// ── 시드 ─────────────────────────────────────────────────────
	/**
	 * 판정용 난수 스트림을 초기화한다 (서버 전용). GameState 가 덱 시드를 정할 때 함께 부른다.
	 *
	 * ⚠️ 셔플과 스트림을 공유하지 않는다. 한 스트림을 같이 쓰면 판정을 한 번 더 굴리는 순간
	 *    이후 셔플 결과가 통째로 밀려서, 코드를 조금만 고쳐도 같은 시드의 판이 재현되지 않고
	 *    플레이어가 판정을 유도해 다음 뽑기를 흔들 여지도 생긴다.
	 *    그래서 같은 매치 시드에서 파생하되(재현 가능) 스트림은 따로 둔다.
	 */
	void InitializeStream(int32 MatchSeed);

	// ── 판정 (구형 경로) ─────────────────────────────────────────
	//
	// ⚠️ [비활성] 아래 셋(RollDice / ClassifyRoll / RollForOutcome)과 위의 *Ratio 네 개는
	//    **컴포넌트 기본 비율**로 판정하는 구형 경로다. 유일한 호출처가 비활성 GA_Missed 하나뿐이라
	//    실제 게임에서는 한 번도 돌지 않는다. 정식 경로는 아래 RollOutcome(카드 비율 기반)이다.
	//
	// ⚠️⚠️ **눈의 방향이 정식 경로와 반대다.** 되살리기 전에 반드시 볼 것:
	//        · ClassifyRoll  : 낮은 눈 = 대실패 … 높은 눈 = 대성공
	//        · RollOutcome   : 낮은 눈 = 대성공 … 높은 눈 = 대실패  (GDD §4.2 누적 순서)
	//      새 카드가 실수로 이 경로를 타면 판정이 **뒤집힌 채로 조용히 동작한다** —
	//      확률 표시(§10)와 실측(§11)이 어긋나는데 크래시도 경고도 없어서 찾기 어렵다.
	//      되살릴 때는 ClassifyRoll 의 누적 순서를 RollOutcome 에 맞춰 뒤집는 것이 먼저다.
	//
	// 삭제하지 않는 이유는 §13.1 비활성 정책과 같다. GA_Missed 를 되살리는 선택지를 남긴다.

	// [비활성] 주사위를 굴려 1~Faces 결과를 반환한다 (서버 전용). Faces<=0 이면 DiceFaces 사용.
	UFUNCTION(BlueprintCallable, Category = "BG|Dice|Deprecated")
	int32 RollDice(int32 Faces = 0);

	// [비활성] 주어진 눈을 비율 구간으로 판정한다 (순수 계산). 위 방향 경고 참고.
	UFUNCTION(BlueprintPure, Category = "BG|Dice|Deprecated")
	EBGDiceOutcome ClassifyRoll(int32 Roll, int32 Faces = 0) const;

	/**
	 * [비활성] 주사위를 굴려 판정 결과를 반환한다 (굴린 눈은 OutRoll). 서버 전용.
	 *   RollBonus: 판정에만 더해지는 보정치(행운·민첩 등). OutRoll(원본 눈)은 그대로 두고,
	 *              (OutRoll + RollBonus) 로 구간을 판정한다. 값이 커지면 상위 등급이 잘 뜬다.
	 *
	 * 호출처는 비활성 GA_Missed 뿐이다. 새 코드에서 부르지 말 것 — 위 방향 경고 참고.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Dice|Deprecated")
	EBGDiceOutcome RollForOutcome(int32& OutRoll, int32 Faces = 0, int32 RollBonus = 0);

	// 판정 결과 → 화면/로그용 텍스트 ("대실패"/"실패"/"성공"/"대성공"). 출력용.
	UFUNCTION(BlueprintPure, Category = "BG|Dice")
	static FText GetOutcomeText(EBGDiceOutcome Outcome);

	// ══════════════════════════════════════════════════════════════════
	//  카드 데이터 기반 판정 (GDD §4.2) — 신규 정식 경로
	//
	//  위의 RollForOutcome 은 컴포넌트 기본 비율을 쓰는 구형 경로다.
	//  카드마다 비율이 다르므로 아래를 쓴다: 카드 비율 + 보정 → 정규화 → 0~99 난수 1회.
	// ══════════════════════════════════════════════════════════════════

	//	행운 1점이 성공 / 대성공 비율에 더하는 값. GDD §4.2 "보정은 비율에 더한다".
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BG|Dice", meta = (ClampMin = "0"))
	int32 LuckSuccessWeightPerPoint = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BG|Dice", meta = (ClampMin = "0"))
	int32 LuckCriticalWeightPerPoint = 2;

	/** 행운 보정을 비율에 반영한 결과를 돌려준다 (순수 계산 — 클라의 확률 미리보기에도 쓴다). */
	UFUNCTION(BlueprintPure, Category = "BG|Dice")
	FBGOutcomeWeights ApplyLuck(const FBGOutcomeWeights& Base, int32 Luck) const;

	/**
	 * 시전자의 모든 보정(행운 + 비율 보정 어트리뷰트)을 반영한 최종 비율.
	 *
	 * 적용 순서 (GDD §14 "보정 합산 방식" 확정안):
	 *   기본 비율 × 승산(WeightMult*) + 가산(WeightBonus* + 행운) → 0 클램프
	 *
	 * 순수 계산이라 클라에서도 안전하다 — GDD §10 의 "최종 확률 표시" 에 그대로 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Dice")
	FBGOutcomeWeights ApplyModifiers(const FBGOutcomeWeights& Base,
		const UAbilitySystemComponent* SourceASC) const;

	/**
	 * 비율로 판정한다 (서버 전용). OutRoll 은 0~99 난수 원본.
	 * 비율 합이 0 이면 경고 후 Failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Dice")
	EBGDiceOutcome RollOutcome(const FBGOutcomeWeights& Weights, int32& OutRoll);

	/**
	 * 비율 → 실제 확률(%)로 변환한다 (순수 계산). GDD §10 확률 표시용.
	 * 서버 판정 없이 클라에서 호출해도 안전하다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Dice")
	static void GetOutcomeChances(const FBGOutcomeWeights& Weights,
		float& OutCriticalFailure, float& OutFailure, float& OutSuccess, float& OutCriticalSuccess);

	/**
	 * 0 ~ Max-1 정수 (서버 전용). Max<=0 이면 INDEX_NONE.
	 *
	 * 판정 외의 무작위 결정(무작위 카드 선택 등)도 이 스트림을 쓴다 —
	 * 셔플 스트림과 분리되어 있으므로 덱 순서를 밀지 않고, 같은 시드에서 재현된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Dice")
	int32 RandomIndex(int32 Max);

	// ── 결과 ↔ 태그 변환 (Resolution.*) ──
	//    판정 주체가 여기 하나이므로 변환도 여기 모은다.
	UFUNCTION(BlueprintPure, Category = "BG|Dice")
	static FGameplayTag OutcomeToTag(EBGDiceOutcome Outcome);

	/** 태그 컨테이너에서 Resolution.* 를 찾아 결과로 되돌린다. 없으면 Fallback. */
	static EBGDiceOutcome TagsToOutcome(const FGameplayTagContainer& Tags, EBGDiceOutcome Fallback);

private:
	// 판정 전용 시드 스트림 (서버에서만 굴린다). 셔플 스트림과 분리 — 위 InitializeStream 주석 참고.
	FRandomStream RandomStream;

	// InitializeStream 이 아직 안 불렸으면 true. 이 상태로 굴리면 경고를 남긴다.
	bool bStreamInitialized = false;
};

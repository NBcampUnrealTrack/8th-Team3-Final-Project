// BANG! 카드 인스턴스 → UI 뷰모델 변환. 게임 로직과 UI 의 유일한 접점이므로 한곳에 모은다.
//
//   FBGCardInstance (게임 로직: InstanceId + CardId)
//        + DT_BGCard 의 FBGCardRow (이름/설명/특성)
//   → FBGCardView   (UI: 화면에 그릴 것 전부)
//
// UI 계층(UBGHandWidget 등)은 FBGCardInstance 도 DT 도 모른다. 그 경계를 이 파일이 지킨다.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/View/BGCardView.h"
#include "UI/View/BGCardFXTypes.h"   // FBGCardFXStep
#include "BGCardViewLibrary.generated.h"

struct FBGCardInstance;
struct FBGCardRow;
class ABGPlayerState;
class UAbilitySystemComponent;

UCLASS()
class BG_API UBGCardViewLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 카드 인스턴스 1장을 UI 뷰모델로 변환한다.
	 * DT 에서 Row 를 못 찾으면 CardId 를 그대로 이름에 넣어 화면에서 바로 눈에 띄게 한다.
	 *
	 * SourceASC 를 주면 그 시전자의 보정을 반영한 최종 확률(FinalChance)까지 채운다.
	 * 비워 두면 FinalChance 는 BaseChance 와 같아진다 — 남의 카드나 프리뷰처럼
	 * "누가 쓸지" 가 정해지지 않은 자리에서는 보정을 붙일 근거가 없다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Card", meta = (WorldContext = "WorldContextObject"))
	static FBGCardView MakeCardView(const UObject* WorldContextObject, const FBGCardInstance& Card,
		const UAbilitySystemComponent* SourceASC = nullptr);

	/** 좁은 표시 공간용 축약값 — "성공+대성공" (GDD §10). */
	UFUNCTION(BlueprintPure, Category = "BG|Card")
	static float GetSuccessOrBetter(const FBGCardChanceView& Chance) { return Chance.GetSuccessOrBetter(); }

	/**
	 * 확률(%)에서 0~99 굴림 값의 등급 경계를 뽑는다. 주사위 눈금·게이지를 그릴 때 쓴다.
	 *
	 * 높을수록 좋다 — RollOutcome 이 대실패부터 누적 비교한다.
	 *   Roll <= OutCriticalFailureMax → 대실패 / <= OutFailureMax → 실패
	 *   / <= OutSuccessMax → 성공 / 그 위 → 대성공
	 * 확률이 0 인 등급은 앞 경계와 같아진다(구간 없음).
	 *
	 * ⚠️ 표시용 근사값이다. 서버는 정규화 전 가중치로 판정하므로 경계에 딱 걸린 눈이
	 *    한 칸 어긋날 수 있다. 등급의 진실은 언제나 Context.Tier 다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Card")
	static void GetRollBands(const FBGCardChanceView& Chance,
		int32& OutCriticalFailureMax, int32& OutFailureMax, int32& OutSuccessMax);

	/**
	 * 손패 전체를 변환한다.
	 * ⚠️ Hand 는 COND_OwnerOnly 라, 클라에서 다른 플레이어의 PlayerState 를 넘기면 빈 배열이 나온다.
	 *    이는 버그가 아니라 은닉 정보가 정상 동작한다는 뜻이다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Card")
	static TArray<FBGCardView> MakeHandViews(const ABGPlayerState* PlayerState);

	/**
	 * 발동한 등급의 효과 목록을 연출 스텝 목록으로 바꾼다.
	 *
	 * **연출용 DT 를 따로 두지 않는 이유가 이 함수다.** 카드가 실제로 한 일이 곧 연출
	 * 구성이므로, 효과를 바꾸면 연출이 저절로 따라온다 — 두 곳에 적혀 어긋날 일이 없다.
	 *
	 * ⚠️ 게임 로직 타입(FBGCardEffect / EBGCardEffectOp)은 여기서 끊긴다.
	 *    UI 계층으로는 FBGCardFXStep 만 넘어간다.
	 */
	static TArray<FBGCardFXStep> MakeFXSteps(const FBGCardRow& Row, EBGResolutionTier Tier);

	/**
	 * 카드 대분류(장비/공격/마법)를 Row 에서 유도한다. 유도 규칙은 EBGCardCategory 주석 참고.
	 *
	 * MakeCardView 안에 있던 것을 꺼낸 것이다. 손패 뷰를 만들 수 없는 자리 —
	 * 판정 통지처럼 카드 "종류" 만 아는 곳 — 에서도 같은 분류가 필요해졌기 때문이다.
	 *
	 * ⚠️ 여기 말고 다른 곳에서 다시 유도하지 말 것. 두 곳으로 갈라지면 카드 UI 와
	 *    판정 UI 가 같은 카드를 다른 종류로 그린다.
	 *
	 * ⚠️ MakeFXSteps 와 같은 이유로 UFUNCTION 이 아니다 — FBGCardRow 는 게임 로직 타입이라
	 *    BP 로 넘기지 않는다. BP 는 뷰에 실려 온 Category 를 읽는다.
	 */
	static EBGCardCategory DeriveCategory(const FBGCardRow& Row);
};

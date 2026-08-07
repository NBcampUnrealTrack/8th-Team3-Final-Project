// 범용 카드 효과 실행기 — 카드 데이터(FBGOutcomeEffects)에 적힌 효과 목록을 그대로 실행한다.
//
// 왜 범용인가:
//   카드마다 GA 를 만들면 "카드 1장 뽑고 피해 1 주고 한도 1 회복" 같은 조합마다 C++ 을 고쳐야 한다.
//   효과를 데이터로 표현하면 새 카드는 DT 행 추가만으로 끝난다(GDD §6.4 신규 카드 확장 전제).
//
// 사용법:
//   1) DT_BGCard 의 OutcomeEffects 에 등급별 효과를 채운다
//   2) AbilityByCardId 에서 그 카드를 이 클래스로 매핑한다
//
// 판정은 하지 않는다 — 서버가 이미 끝냈고 Resolution.* 태그로 등급이 전달된다 (GDD §9.1).
// 어느 카드인지는 페이로드의 Card.Id.* 태그로 알아내 효과 목록을 조회한다.

#pragma once

#include "CoreMinimal.h"
#include "GAS/BGGameplayAbility.h"
#include "Card/BGGameDataTypes.h"   // EBGDiceOutcome / FBGCardEffect
#include "GA_BGCardEffects.generated.h"

class ABGGameState;
class ABGPlayerState;
class UAbilitySystemComponent;

UCLASS()
class BG_API UGA_BGCardEffects : public UBGGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BGCardEffects();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// [비활성] 힘 배율 / 지능 경감 — 참조하지 않는다 (GDD §7.1 / §13.1).
	//   피해는 카드 데이터(OutcomeEffects)가 전부 결정한다. 스탯을 되살릴 때 여기부터 본다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Damage|Deprecated", meta = (ClampMin = "0.0"))
	float StrengthDamageMult = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Damage|Deprecated", meta = (ClampMin = "0.0"))
	float IntelligenceMitigation = 0.5f;

	/** 효과 목록을 순서대로 실행하고 사람이 읽을 요약을 채운다. */
	void ExecuteEffects(
		const TArray<FBGCardEffect>& Effects,
		AActor* Avatar,
		AActor* TargetActor,
		UAbilitySystemComponent* SourceASC,
		TArray<FString>& OutSummary);

	// 개별 효과 — 성공하면 요약 문구를 돌려준다(빈 문자열이면 아무 일도 없었음).
	FString ApplyDamage(int32 Amount, AActor* Avatar, AActor* TargetActor, UAbilitySystemComponent* SourceASC);
	FString ApplyHeal(int32 Amount, UAbilitySystemComponent* SourceASC);
	FString ApplyDraw(int32 Amount, ABGGameState* GS, ABGPlayerState* SelfPS);

	/**
	 * 대상에게서 카드를 무작위로 떼어낸다 — 손패와 장비를 한 함수가 처리한다.
	 *
	 * 두 경로가 다른 것은 셋뿐이다: 어느 배열에서 고르는가, 무엇을 키로 빼는가, 요약 문구.
	 * 나머지(반복마다 목록 재조회 · 제거 전 값 복사 · 훔침/버림 분기)는 완전히 같아서,
	 * 따로 두면 한쪽만 고쳐지는 자리가 된다.
	 *
	 * bSteal 이면 가져온 카드가 시전자의 **손패**로 들어간다 — 장비여도 장비 칸에 바로 꽂지 않는다.
	 * 그래야 "장착" 이 카드 사용 횟수를 소비하는 규칙(GDD §5.3)을 우회하지 않는다.
	 */
	FString ApplyTakeFromTarget(int32 Amount, bool bSteal, bool bFromEquipment,
		ABGGameState* GS, ABGPlayerState* SelfPS, ABGPlayerState* TargetPS, AActor* Avatar);

	FString ApplyRestoreCardUse(int32 Amount, ABGPlayerState* SelfPS);
	FString ApplyStatusEffect(const FBGCardEffect& Effect, ABGPlayerState* SelfPS, ABGPlayerState* TargetPS);

	/** 판정 결과와 실행된 효과 요약을 알린다(연출·UI용). 서버에서 호출. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG")
	void OnCardEffectsResolved(EBGDiceOutcome Outcome, const FString& Summary, AActor* Target);
};

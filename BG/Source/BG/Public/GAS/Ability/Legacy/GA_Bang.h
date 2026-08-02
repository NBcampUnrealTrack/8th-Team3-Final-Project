// BANG! — 대상 1명에게 피해. 판정은 서버가 카드 DT(DT_BGCard)로 끝내고, 이 GA 는 결과만 실행한다.
//   등급별 기본 피해는 카드 DT 의 OutcomeMagnitudes 에서 온다(GDD §4.3).
//   여기서 붙는 보정은 피해 계산뿐이다: 힘 → 피해 배율, 대상의 지능 → 받는 피해 경감.
//   판정 확률에 붙는 보정(행운 등)은 없다 — GDD §7.1 확정 스탯이 아니고 §14 미결정이다.
//
// 대상은 TriggerEventData->Target 으로 전달된다(응답 창/타깃팅은 §3에서 확장).
// 대상이 없으면(단독 테스트) 계산된 피해를 로그로만 남기고 적용은 건너뛴다.

#pragma once

#include "CoreMinimal.h"
#include "GAS/BGGameplayAbility.h"
#include "Card/BGGameDataTypes.h"   // EBGDiceOutcome
#include "GA_Bang.generated.h"

UCLASS()
class BG_API UGA_Bang : public UBGGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Bang();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// [비활성] 자체 판정 폴백용 등급 피해(스탯 보정 전). 실제 수치는 카드 DT 의 OutcomeMagnitudes 다.
	//   선언만 유지하고 참조하지 않는다 (GDD §13.1). 되살리려면 TierBaseDamage 호출을 복구한다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Damage")
	int32 CriticalSuccessDamage = 2;   // 크리티컬

	UPROPERTY(EditDefaultsOnly, Category = "BG|Damage")
	int32 SuccessDamage = 1;           // 기본

	UPROPERTY(EditDefaultsOnly, Category = "BG|Damage")
	int32 FailureDamage = 1;           // 경감(맞긴 함)

	UPROPERTY(EditDefaultsOnly, Category = "BG|Damage")
	int32 CriticalFailureDamage = 0;   // 빗나감

	// 힘 1당 피해 배율 증가분. 최종 배율 = 1 + Strength * StrengthDamageMult.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Damage", meta = (ClampMin = "0.0"))
	float StrengthDamageMult = 0.25f;

	// 대상 지능 1당 경감되는 피해량(정수 반올림 후 차감).
	UPROPERTY(EditDefaultsOnly, Category = "BG|Damage", meta = (ClampMin = "0.0"))
	float IntelligenceMitigation = 0.5f;

	// 판정/피해가 확정됐을 때(연출·UI용) 방송. 서버에서 호출.
	UFUNCTION(BlueprintImplementableEvent, Category = "BG")
	void OnBangResolved(EBGDiceOutcome Outcome, int32 FinalDamage, AActor* Target);

	// 등급 → 기본 피해(스탯 보정 전) 매핑.
	int32 TierBaseDamage(EBGDiceOutcome Outcome) const;
};

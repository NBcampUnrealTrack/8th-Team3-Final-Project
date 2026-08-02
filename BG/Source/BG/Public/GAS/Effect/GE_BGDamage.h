// 피해 GE (Instant) — Health 를 SetByCaller.Damage 값만큼 가산한다.
//   ⚠️ 값은 "가산"이므로 피해를 줄 때는 시전 쪽에서 음수를 넣는다.
//      예) 최종 피해 2 → SetSetByCallerMagnitude(SetByCaller.Damage, -2.f)
//   경감(지능)·배율(힘)·판정(주사위) 계산은 GA_Bang 이 하고, 이 GE 는 최종 수치만 적용한다.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_BGDamage.generated.h"

UCLASS()
class BG_API UGE_BGDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_BGDamage();
};

// 보안관 보너스 GE (Infinite). Health/MaxHealth +1. 판이 끝날 때까지 유지된다.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_BGSheriffBonus.generated.h"

UCLASS()
class BG_API UGE_BGSheriffBonus : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_BGSheriffBonus();
};

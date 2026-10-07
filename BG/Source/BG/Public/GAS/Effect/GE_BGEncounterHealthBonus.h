// PvE 조우 승리 보상 GE (Infinite). Health/MaxHealth +2. 이긴 조우 수만큼 겹쳐 적용된다.
// 한 번에 +2 만 올린다 - 몇 번 겹칠지는 ABGCharacter::ApplyEncounterHealthBonusIfNeeded 가 정한다.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_BGEncounterHealthBonus.generated.h"

UCLASS()
class BG_API UGE_BGEncounterHealthBonus : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_BGEncounterHealthBonus();

	/** 한 번 적용될 때 올라가는 체력. 이긴 조우마다 이만큼씩 쌓인다. */
	static constexpr float HealthPerStack = 2.f;
};

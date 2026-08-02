// BANG! 전용 ASC. 지금은 UAbilitySystemComponent 그대로지만,
// 응답 창(Resolution) 관련 헬퍼를 여기 확장해 나간다 (Team4 의 BaseASC 와 동일 구조).

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "BGAbilitySystemComponent.generated.h"

UCLASS()
class BG_API UBGAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
};

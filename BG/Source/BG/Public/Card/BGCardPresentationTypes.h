#pragma once

#include "CoreMinimal.h"
#include "BGCardPresentationTypes.generated.h"

/** 카드가 대상 선택 단계에서 사용할 선 연출의 시각 계열. 게임 규칙과는 무관하다. */
UENUM(BlueprintType)
enum class EBGTargetSelectionStyle : uint8
{
	Basic    UMETA(DisplayName = "기본"),
	Magic    UMETA(DisplayName = "마법"),
	Physical UMETA(DisplayName = "물리")
};

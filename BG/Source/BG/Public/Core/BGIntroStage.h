// 도입 연출의 단계. GameState 가 소유하고 복제하지만, 타입만 필요한 곳이 많아 따로 둔다
// (PlayerController·HUD 가 단계 하나 때문에 BGGameState.h 전체를 끌어오지 않게 한다).

#pragma once

#include "CoreMinimal.h"
#include "BGIntroStage.generated.h"

/**
 * 판이 열리기 전의 도입 연출 단계.
 *
 * EBGMatchEndStage 와 같은 이유로 **상태**다 — 늦게 들어온 클라도 지금 어느 단계인지 받는다.
 * 각 단계는 서버 타이머로 자동으로 넘어간다. 기다려 주지 않는다: 한 사람의 입력을 기다리면
 * 그 사람이 늦을 때 전원이 멈춘다.
 *
 * 흐름: Campfire(야영지 진입) → RoleReveal(내 역할) → HeroReveal(용사 공개)
 *       → FirstTurn(첫 턴 안내) → None(판 시작)
 *
 * ⚠️ 이 단계가 도는 동안 규칙은 아직 돌지 않는다. 손패 배분도 첫 턴(밧줄 점화)도
 *    연출이 **끝난 뒤에** 이뤄진다 — 연출 중에 나눠 주면 아직 열리지 않은 판의 카드가 손에 들린다.
 */
UENUM(BlueprintType)
enum class EBGIntroStage : uint8
{
	None       UMETA(DisplayName = "도입 아님"),
	Campfire   UMETA(DisplayName = "야영지 진입"),
	RoleReveal UMETA(DisplayName = "내 역할 확인"),
	HeroReveal UMETA(DisplayName = "용사 공개"),
	FirstTurn  UMETA(DisplayName = "첫 턴 안내")
};

/** 도입 연출 단계가 바뀌었다. 서버·클라 양쪽에서 불린다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBGIntroStageChanged, EBGIntroStage /*Stage*/);

// 메인메뉴 전용 게임모드. 세션·좌석·판 진행이 전혀 없는 UI 전용 레벨을 위한 것이다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BGMainMenuGameMode.generated.h"

UCLASS()
class BG_API ABGMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABGMainMenuGameMode();
};

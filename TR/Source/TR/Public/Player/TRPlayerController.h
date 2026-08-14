//TRPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TRPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class TR_API ATRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 치트: 몬스터 자원 지급. 콘솔에서  TR_AddResource 50 - 인자 생략 시 1000 지급.
	UFUNCTION(Exec)
	void TR_AddResource(int32 Amount = 1000);

private:
	UFUNCTION(Server, Reliable)
	void ServerCheatAddResource(int32 Amount);
};

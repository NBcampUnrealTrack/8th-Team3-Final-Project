// TRGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TRGameState.generated.h"

UCLASS()
class TR_API ATRGameState : public AGameStateBase
{
	GENERATED_BODY()
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	int32 MonsterResource = 0;
};

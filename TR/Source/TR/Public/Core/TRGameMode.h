// TRGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TRGameMode.generated.h"

UCLASS()
class TR_API ATRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/**
	 * 웨이브를 시작한다. 호스트가 로비에서 시작을 누를 때 부른다.
	 * 준비가 덜 됐거나 이미 시작했으면 아무것도 하지 않고 false.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Lobby")
	bool StartMatch();

protected:
	virtual void PostLogin(APlayerController* NewPlayer) override;
};

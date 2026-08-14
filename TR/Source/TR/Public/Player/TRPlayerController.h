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
	/** 로비 준비 상태를 바꾼다. 어느 쪽에서 불러도 서버로 넘어간다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Lobby")
	void SetLobbyReady(bool bInReady);

	/** 호스트가 판을 시작한다. 호스트가 아니면 서버가 무시한다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Lobby")
	void RequestStartMatch();

private:
	UFUNCTION(Server, Reliable)
	void ServerSetLobbyReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void ServerStartMatch();
};

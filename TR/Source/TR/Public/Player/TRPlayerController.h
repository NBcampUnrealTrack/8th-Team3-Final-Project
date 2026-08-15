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

	// 치트: 모든 장갑·포탑과 기차 선체에 같은 피해를 준다. 콘솔에서  TR_DamageAll 300 - 인자 생략 시 100.
	UFUNCTION(Exec)
	void TR_DamageAll(float Damage = 100.f);

	/** 로비 준비 상태를 바꾼다. 어느 쪽에서 불러도 서버로 넘어간다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Lobby")
	void SetLobbyReady(bool bInReady);

	/** 호스트가 판을 시작한다. 호스트가 아니면 서버가 무시한다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Lobby")
	void RequestStartMatch();

private:
	UFUNCTION(Server, Reliable)
	void ServerCheatAddResource(int32 Amount);

	UFUNCTION(Server, Reliable)
	void ServerCheatDamageAll(float Damage);

	UFUNCTION(Server, Reliable)
	void ServerSetLobbyReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void ServerStartMatch();
};

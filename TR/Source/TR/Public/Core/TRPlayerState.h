// TRPlayerState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TRPlayerState.generated.h"

UCLASS()
class TR_API ATRPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "Supply")
	bool HasSupplySpace() const { return CarriedSupply < MaxCarriedSupply; }
	
	static constexpr int32 MaxCarriedSupply = 6;
	
	void AddCarriedSupply(int32 Amount);
	
	int32 GetCarriedSupply() const { return CarriedSupply; }
	
	void ConsumeCarriedSupply(int32 Amount);

	/** 로비에서 준비를 눌렀는지. */
	UFUNCTION(BlueprintPure, Category = "TR|Lobby")
	bool IsReady() const { return bIsReady; }

	/** 방을 만든 사람인지. 시작 권한과 준비 집계 제외에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "TR|Lobby")
	bool IsHost() const { return bIsHost; }

	/** 서버에서만 부른다. */
	void SetReady(bool bInReady);

	/** 서버에서만 부른다. 게임모드가 첫 로컬 접속자에게 매긴다. */
	void SetHost(bool bInHost);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	int32 CarriedSupply = 6;

	UPROPERTY(Replicated)
	bool bIsReady = false;

	UPROPERTY(Replicated)
	bool bIsHost = false;
};

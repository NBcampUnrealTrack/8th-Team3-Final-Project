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
	
	UPROPERTY(Replicated)
	int32 CarriedSupply = 0;
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

// TRGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TRGameState.generated.h"


UCLASS()
class TR_API ATRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	int32 GetMonsterResource() const { return MonsterResource; }

	void ConsumeMonsterResource(int32 Amount);
	
	bool TryConsumeMonsterResource(int32 Amount);
	
	int32 GetArmorLevel() const { return ArmorLevel; }
	int32 GetTurretLevel() const { return TurretLevel; }
	
	bool UpgradeArmorLevel();
	bool UpgradeTurretLevel();

	static constexpr int32 MaxArmorLevel = 4;
	static constexpr int32 MaxTurretLevel = 3;
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	int32 MonsterResource = 0;
	
	UPROPERTY(Replicated)
	int32 ArmorLevel = 1;

	UPROPERTY(Replicated)
	int32 TurretLevel = 1;
};

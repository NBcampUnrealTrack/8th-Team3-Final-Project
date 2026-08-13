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

	// 검사와 차감을 함께 한다. 부족하면 아무것도 깎지 않고 false.
	// 호출부에서 따로 차감하지 말 것 — 이중 차감 버그의 원인이었다.
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

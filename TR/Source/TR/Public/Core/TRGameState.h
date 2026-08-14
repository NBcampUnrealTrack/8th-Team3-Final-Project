// TRGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TRGameState.generated.h"

class UTRActionComponent;

UCLASS()
class TR_API ATRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	int32 GetMonsterResource() const { return MonsterResource; }

	// 검사와 차감을 함께 한다. 부족하면 아무것도 깎지 않고 false.
	bool TryConsumeMonsterResource(int32 Amount);
	
	void AddMonsterResource(int32 Amount);
	
	int32 GetArmorLevel() const { return ArmorLevel; }
	int32 GetTurretLevel() const { return TurretLevel; }
	
	bool UpgradeArmorLevel();
	bool UpgradeTurretLevel();

	static constexpr int32 MaxArmorLevel = 4;
	static constexpr int32 MaxTurretLevel = 3;

	// 재설치는 열차 전체에서 한 번에 하나만 돈다. 진행 중인 컴포넌트, 없으면 nullptr.
	// 소유 액터가 사라지면 저절로 nullptr 이 된다.
	UTRActionComponent* GetBuildingComponent() const;

	// 서버 전용. 갱신 지점을 하나로 두려고 UTRActionComponent 가 자기 상태를 바꿀 때만 부른다.
	void SetBuildingComponent(UTRActionComponent* InComponent);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	int32 MonsterResource = 0;
	
	UPROPERTY(Replicated)
	int32 ArmorLevel = 1;

	UPROPERTY(Replicated)
	int32 TurretLevel = 1;

	UPROPERTY(Replicated)
	TObjectPtr<UTRActionComponent> BuildingComponent;
};

// TRGameState.cpp


#include "Core/TRGameState.h"
#include "Interaction/TRActionComponent.h"
#include "Net/UnrealNetwork.h"

void ATRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRGameState, MonsterResource);
	DOREPLIFETIME(ATRGameState, ArmorLevel);
	DOREPLIFETIME(ATRGameState, TurretLevel);
	DOREPLIFETIME(ATRGameState, BuildingComponent);
}

UTRActionComponent* ATRGameState::GetBuildingComponent() const
{
	// 짓던 플레이어가 나가면 컴포넌트가 사라지므로 여기서 걸러 준다.
	UTRActionComponent* Component = BuildingComponent.Get();
	return IsValid(Component) ? Component : nullptr;
}

void ATRGameState::SetBuildingComponent(UTRActionComponent* InComponent)
{
	if (!HasAuthority())
	{
		return;
	}

	BuildingComponent = InComponent;
}

bool ATRGameState::TryConsumeMonsterResource(int32 Amount)
{
	if (!HasAuthority())
	{
		return false;
	}
	
	if (MonsterResource < Amount)
	{
		return false;
	}
	MonsterResource -= Amount;
	return true;
}

void ATRGameState::AddMonsterResource(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0)
	{
		return;
	}
	MonsterResource += Amount;
}

bool ATRGameState::UpgradeArmorLevel()
{
	if (!HasAuthority())
	{
		return false;
	}
	
	if (ArmorLevel >= MaxArmorLevel)
	{
		return false;
	}
	++ArmorLevel;
	return true;
}

bool ATRGameState::UpgradeTurretLevel()
{
	if (!HasAuthority())
	{
		return false;
	}
	
	if (TurretLevel >= MaxTurretLevel)
	{
		return false;
	}
	++TurretLevel;
	return true;
}

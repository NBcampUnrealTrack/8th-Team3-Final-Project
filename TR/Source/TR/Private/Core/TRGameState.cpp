// TRGameState.cpp


#include "Core/TRGameState.h"
#include "Net/UnrealNetwork.h"

void ATRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRGameState, MonsterResource);
	DOREPLIFETIME(ATRGameState, ArmorLevel);
	DOREPLIFETIME(ATRGameState, TurretLevel);
}

void ATRGameState::ConsumeMonsterResource(int32 Amount)
{
	if (!HasAuthority())
	{
		return;
	}
	
	MonsterResource = FMath::Max(0, MonsterResource - Amount);
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

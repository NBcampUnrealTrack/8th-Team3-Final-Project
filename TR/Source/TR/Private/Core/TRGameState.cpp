// TRGameState.cpp


#include "Core/TRGameState.h"
#include "Net/UnrealNetwork.h"

void ATRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRGameState, MonsterResource);
}

void ATRGameState::ConsumeMonsterResource(int32 Amount)
{
	MonsterResource = FMath::Max(0, MonsterResource - Amount);
}

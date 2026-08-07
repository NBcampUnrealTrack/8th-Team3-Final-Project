// TRGameState.cpp


#include "Core/TRGameState.h"
#include "Net/UnrealNetwork.h"

void ATRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRGameState, MonsterResource);
}

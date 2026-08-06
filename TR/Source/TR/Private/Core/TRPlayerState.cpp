// TRPlayerState.cpp


#include "Core/TRPlayerState.h"
#include "Net/UnrealNetwork.h"

void ATRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRPlayerState, CarriedSupply);
}

// TRPlayerState.cpp


#include "Core/TRPlayerState.h"
#include "Net/UnrealNetwork.h"


void ATRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRPlayerState, CarriedSupply);
}

void ATRPlayerState::AddCarriedSupply(int32 Amount)
{
	CarriedSupply = FMath::Clamp(CarriedSupply + Amount, 0, MaxCarriedSupply);
}

void ATRPlayerState::ConsumeCarriedSupply(int32 Amount)
{
	CarriedSupply = FMath::Max(0, CarriedSupply - Amount);
}

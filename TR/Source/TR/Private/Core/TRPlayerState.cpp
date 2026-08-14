// TRPlayerState.cpp


#include "Core/TRPlayerState.h"
#include "Net/UnrealNetwork.h"


void ATRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRPlayerState, CarriedSupply);
	DOREPLIFETIME(ATRPlayerState, bIsReady);
	DOREPLIFETIME(ATRPlayerState, bIsHost);
}

void ATRPlayerState::AddCarriedSupply(int32 Amount)
{
	CarriedSupply = FMath::Clamp(CarriedSupply + Amount, 0, MaxCarriedSupply);
}

void ATRPlayerState::ConsumeCarriedSupply(int32 Amount)
{
	CarriedSupply = FMath::Max(0, CarriedSupply - Amount);
}

void ATRPlayerState::SetReady(bool bInReady)
{
	if (!ensureMsgf(HasAuthority(), TEXT("준비 상태는 서버에서만 바꿀 수 있습니다.")))
	{
		return;
	}

	bIsReady = bInReady;
}

void ATRPlayerState::SetHost(bool bInHost)
{
	if (!ensureMsgf(HasAuthority(), TEXT("호스트 표시는 서버에서만 매길 수 있습니다.")))
	{
		return;
	}

	bIsHost = bInHost;
}

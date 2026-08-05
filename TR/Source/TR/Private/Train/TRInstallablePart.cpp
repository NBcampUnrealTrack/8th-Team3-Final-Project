// TRInstallablePart.cpp


#include "Train/TRInstallablePart.h"
#include "Net/UnrealNetwork.h"

ATRInstallablePart::ATRInstallablePart()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ATRInstallablePart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRInstallablePart, CurrentHP);
}

void ATRInstallablePart::ApplyRepair(float Amount)
{
	CurrentHP = FMath::Clamp(CurrentHP + Amount, 0.f, GetMaxHP());
}

void ATRInstallablePart::ApplyInstall()
{
	CurrentHP = GetMaxHP();
}

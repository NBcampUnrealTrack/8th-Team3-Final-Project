// TRTurret.cpp


#include "Train/TRTurret.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TRTypes.h"

ATRTurret::ATRTurret()
{
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	RootComponent = TurretMesh;
}

void ATRTurret::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRTurret, Occupant);
}

float ATRTurret::GetMaxHP() const
{
	if (LevelTable)
	{
		if (const FTRTurretLevelRow* Row = LevelTable->FindRow<FTRTurretLevelRow>(FName(*FString::FromInt(CurrentLevel)), TEXT("GetMaxHP")))
		{
			return Row->MaxHP;
		}
	}
	return 0.f;
}

int32 ATRTurret::GetInstallCost() const
{
	if (LevelTable)
	{
		if (const FTRTurretLevelRow* Row = LevelTable->FindRow<FTRTurretLevelRow>(FName(*FString::FromInt(CurrentLevel)), TEXT("GetInstallCost")))
		{
			return Row->InstallCost;
		}
	}
	return 0;
}

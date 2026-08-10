// TRTrain.cpp

#include "Train/TRTrain.h"
#include "Train/TRArmor.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ATRTrain::ATRTrain()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;
}

void ATRTrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRTrain, CurrentHullHealth);
}

int32 ATRTrain::GetActiveArmorCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<ATRArmor>& Armor : ArmorSlots)
	{
		if (Armor && !Armor->IsDestroyed())
		{
			++Count;
		}
	}
	return Count;
}

void ATRTrain::ApplyRepair(float Amount)
{
	CurrentHullHealth = FMath::Clamp(CurrentHullHealth + Amount, 0.f, MaxHullHealth);
}

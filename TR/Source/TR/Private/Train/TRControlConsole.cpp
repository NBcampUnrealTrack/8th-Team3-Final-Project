// TRControlConsole.cpp

#include "Train/TRControlConsole.h"

#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ATRControlConsole::ATRControlConsole()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	ConsoleMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConsoleMeshComp"));
	RootComponent = ConsoleMeshComp;
}

void ATRControlConsole::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRControlConsole, Occupant);
}

bool ATRControlConsole::TryOccupy(APawn* NewOccupant)
{
	if (!HasAuthority() || !IsValid(NewOccupant) || IsOccupied())
	{
		return false;
	}

	Occupant = NewOccupant;
	return true;
}

void ATRControlConsole::Release(const APawn* Pawn)
{
	if (!HasAuthority() || !IsOccupiedBy(Pawn))
	{
		return;
	}

	Occupant = nullptr;
}

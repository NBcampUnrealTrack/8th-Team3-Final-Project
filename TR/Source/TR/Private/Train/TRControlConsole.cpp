// TRControlConsole.cpp

#include "Train/TRControlConsole.h"

#include "Components/BoxComponent.h"

ATRControlConsole::ATRControlConsole()
{
	PrimaryActorTick.bCanEverTick = false;
	
	ConsoleMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConsoleMeshComp"));
	RootComponent = ConsoleMeshComp;
}

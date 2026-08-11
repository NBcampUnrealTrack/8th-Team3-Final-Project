// TRControlConsole.cpp

#include "Train/TRControlConsole.h"

ATRControlConsole::ATRControlConsole()
{
	PrimaryActorTick.bCanEverTick = false;
	
	ConsoleMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConsoleMeshComp"));
	RootComponent = ConsoleMeshComp;
}

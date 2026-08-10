// TRControlConsole.cpp

#include "Train/TRControlConsole.h"

#include "Components/BoxComponent.h"

ATRControlConsole::ATRControlConsole()
{
	PrimaryActorTick.bCanEverTick = false;
	
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
}

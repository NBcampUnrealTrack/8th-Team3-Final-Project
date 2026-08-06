// TRSupplyPoint.cpp


#include "Train/TRSupplyPoint.h"
#include "Components/BoxComponent.h"

ATRSupplyPoint::ATRSupplyPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
}



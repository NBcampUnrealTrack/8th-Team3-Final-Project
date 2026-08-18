// TRSupplyPoint.cpp


#include "Train/TRSupplyPoint.h"
#include "Components/BoxComponent.h"
#include "TRTypes.h"

ATRSupplyPoint::ATRSupplyPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
	CollisionBox->SetCollisionProfileName(TRCollisionProfile::Interactable);
}



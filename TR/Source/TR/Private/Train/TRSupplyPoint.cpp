// Fill out your copyright notice in the Description page of Project Settings.


#include "Train/TRSupplyPoint.h"
#include "Components/BoxComponent.h"

ATRSupplyPoint::ATRSupplyPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
}



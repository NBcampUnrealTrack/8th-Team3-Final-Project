// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRSupplyPoint.generated.h"

class UBoxComponent;

UCLASS()
class TR_API ATRSupplyPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATRSupplyPoint();

private:
	UPROPERTY(VisibleAnywhere, Category = "SupplyRoom")
	TObjectPtr<UBoxComponent> CollisionBox;

};

// TRSupplyPoint.h

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
	ATRSupplyPoint();

private:
	UPROPERTY(VisibleAnywhere, Category = "SupplyRoom")
	TObjectPtr<UBoxComponent> CollisionBox;

};

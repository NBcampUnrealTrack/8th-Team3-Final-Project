// TRControlConsole.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRControlConsole.generated.h"

class UBoxComponent;

UCLASS()
class TR_API ATRControlConsole : public AActor
{
	GENERATED_BODY()
	
public:	
	ATRControlConsole();

private:
	UPROPERTY(VisibleAnywhere, Category="ControlConsole")
	TObjectPtr<UStaticMeshComponent> ConsoleMeshComp;
};

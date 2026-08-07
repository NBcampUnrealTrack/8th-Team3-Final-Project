// TRPlayerHUD.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TRPlayerHUD.generated.h"

class UTRPlayerHUDWidget;

UCLASS()
class TR_API ATRPlayerHUD : public AHUD
{
	GENERATED_BODY()

public:
	UTRPlayerHUDWidget* GetPlayerHUDWidget() const { return PlayerHUDWidget; }

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(Transient)
	TObjectPtr<UTRPlayerHUDWidget> PlayerHUDWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTRPlayerHUDWidget> PlayerHUDWidgetClass;
};

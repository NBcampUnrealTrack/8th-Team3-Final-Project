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
	// 생성된 플레이어 HUD 위젯 반환
	UTRPlayerHUDWidget* GetPlayerHUDWidget() const { return PlayerHUDWidget; }

protected:
	virtual void BeginPlay() override;

	//--- 플레이어 HUD 위젯 ---
	UPROPERTY(Transient)
	TObjectPtr<UTRPlayerHUDWidget> PlayerHUDWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTRPlayerHUDWidget> PlayerHUDWidgetClass;
};

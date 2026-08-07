// TRPlayerHUD.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TRPlayerHUD.generated.h"

class UUserWidget;

UCLASS()
class TR_API ATRPlayerHUD : public AHUD
{
	GENERATED_BODY()

public:
	UUserWidget* GetPlayerHUDWidget() const { return PlayerHUDWidget; }

protected:
	virtual void BeginPlay() override;

	// 화면에 띄울 플레이어 HUD 위젯 클래스
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> PlayerHUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PlayerHUDWidget;
};

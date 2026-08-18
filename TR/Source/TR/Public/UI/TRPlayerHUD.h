// TRPlayerHUD.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TRPlayerHUD.generated.h"

class UTRPlayerHUDWidget;
class UTRControlConsoleWidget;
class UUserWidget;

UCLASS()
class TR_API ATRPlayerHUD : public AHUD
{
	GENERATED_BODY()

public:
	UTRPlayerHUDWidget* GetPlayerHUDWidget() const { return PlayerHUDWidget; }

	static void ApplyGameInputMode(APlayerController* PC);

	static void ApplyPopupInputMode(APlayerController* PC, UUserWidget* FocusWidget);

	// 관제석 팝업. 떠 있는 동안은 마우스 커서가 나오고 시점·이동이 잠긴다.
	void ToggleControlConsole();

	void CloseControlConsole();

	bool IsControlConsoleOpen() const { return ControlConsoleWidget != nullptr; }

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	bool bCenterMouse = false;

	UPROPERTY(Transient)
	TObjectPtr<UTRPlayerHUDWidget> PlayerHUDWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTRPlayerHUDWidget> PlayerHUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UTRControlConsoleWidget> ControlConsoleWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTRControlConsoleWidget> ControlConsoleWidgetClass;
};

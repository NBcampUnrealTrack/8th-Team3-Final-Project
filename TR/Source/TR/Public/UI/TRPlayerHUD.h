// TRPlayerHUD.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TRPlayerHUD.generated.h"

class ATRGameState;
class AGameStateBase;
class APawn;
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

	// 폰의 로컬 컨트롤러가 들고 있는 게임플레이 HUD 위젯. 게임 시작 전이거나 원격 폰이면 nullptr.
	static UTRPlayerHUDWidget* FindPlayerHUDWidget(const APawn* Pawn);

	// 팝업을 열 때. 커서를 보이고 중앙 고정을 푼다.
	static void PushPopupInputMode(APlayerController* PC, UUserWidget* FocusWidget);

	// 팝업을 닫을 때. 마지막 팝업이 닫힐 때만 게임 입력 모드로 돌아간다.
	static void PopPopupInputMode(APlayerController* PC);

	// 관제석 팝업. 떠 있는 동안은 마우스 커서가 나오고 시점·이동이 잠긴다.
	void ToggleControlConsole();

	void CloseControlConsole();

	bool IsControlConsoleOpen() const { return ControlConsoleWidget != nullptr; }

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

private:
	// 팝업 수를 건드리지 않고 입력 모드만 바꾼다.
	static void ApplyPopupInputMode(APlayerController* PC, UUserWidget* FocusWidget);

	enum class EScreenPhase : uint8
	{
		None,
		PreMatch,
		Gameplay
	};

	void BindMatchPhase();

	void HandleGameStateSet(AGameStateBase* InGameState);

	void ApplyMatchPhase(bool bStarted);

	void ShowPreMatchScreen();

	void ShowGameplayHUD();

protected:

	bool bCenterMouse = false;

	UPROPERTY(Transient)
	TObjectPtr<UTRPlayerHUDWidget> PlayerHUDWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTRPlayerHUDWidget> PlayerHUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UTRControlConsoleWidget> ControlConsoleWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTRControlConsoleWidget> ControlConsoleWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> PreMatchWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PreMatchWidget;

private:
	TWeakObjectPtr<ATRGameState> BoundGameState;

	FDelegateHandle GameStateSetHandle;

	EScreenPhase AppliedPhase = EScreenPhase::None;

	// 팝업 카운터를 정확히 한 번씩 짝지었는가. Push/Pop 판단은 위젯 포인터가 아니라 이 값만 본다.
	bool bPreMatchInputSuppressed = false;
};

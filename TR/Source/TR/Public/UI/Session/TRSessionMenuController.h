// TR 세션 메뉴 수명 관리 — 토글과 트래블 복구를 맡는다.
// 트래블은 뷰포트 위젯을 파괴하므로 열려 있었는지 기억했다가 맵 로드 후 되살린다.
//
// 위젯을 언제 띄울지만 정한다. 방을 만들고 찾는 일은 UTRSessionFlow 가 한다.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TRSessionMenuController.generated.h"

class UTRSessionMenuWidget;

UCLASS(Config = Game)
class TR_API UTRSessionMenuController : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "TR|UI")
	void Toggle();

	UFUNCTION(BlueprintCallable, Category = "TR|UI")
	void Open();

	UFUNCTION(BlueprintCallable, Category = "TR|UI")
	void Close();

	UFUNCTION(BlueprintPure, Category = "TR|UI")
	bool IsOpen() const { return Menu != nullptr; }

	/** 위젯이 Escape 말고 어떤 키로도 닫히게 하려면 이 값을 본다. */
	UFUNCTION(BlueprintPure, Category = "TR|UI")
	FKey GetDebugToggleKey() const { return DebugToggleKey; }

protected:
	// 띄울 WBP. 다른 것을 쓰려면 DefaultGame.ini 나 에디터에서 이 값만 바꾸면 된다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "TR|UI")
	TSoftClassPtr<UTRSessionMenuWidget> MenuWidgetClass =
		TSoftClassPtr<UTRSessionMenuWidget>(FSoftObjectPath(
			TEXT("/Game/UI/WBP_TRSessionMenu.WBP_TRSessionMenu_C")));

	/**
	 * 열린 메뉴를 닫는 키. 실제로 여는 것은 DefaultInput.ini 의 DebugExecBindings 가
	 * TR_SessionUI 명령에 걸어 준다 — 코드가 직접 키를 잡지 않는다.
	 * 개인적으로 끄려면 그 ini 한 줄만 지우면 된다.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TR|UI|Debug")
	FKey DebugToggleKey = EKeys::F9;

private:
	// 트래블 직전에 위젯을 정상적으로 닫아 입력 모드와 커서를 되돌린다.
	void HandlePreLoadMap(const FString& MapName);

	// 트래블로 위젯이 사라지면 여기서 되살린다. 게임 레벨에서는 되살리지 않는다.
	void HandlePostLoadMap(UWorld* LoadedWorld);

	UPROPERTY(Transient)
	TObjectPtr<UTRSessionMenuWidget> Menu;

	// 트래블 직전에 열려 있었는가. 복구 판단에만 쓴다.
	bool bWasOpen = false;

	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle PreLoadMapHandle;
	FTimerHandle RestoreTimer;
};

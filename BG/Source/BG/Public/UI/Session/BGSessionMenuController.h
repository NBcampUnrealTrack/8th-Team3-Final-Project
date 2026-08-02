// BANG! 세션 메뉴 수명 관리 — 토글과 트래블 복구를 맡는다.
// 트래블은 뷰포트 위젯을 파괴하므로 열려 있었는지 기억했다가 맵 로드 후 되살린다.
// 게임 레벨에 도착하면 되살리지 않는다(메뉴는 로비 전용).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BGSessionMenuController.generated.h"

class UBGSessionMenuWidget;

UCLASS(Config = Game)
class BG_API UBGSessionMenuController : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void Toggle();

	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void Open();

	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void Close();

protected:
	// 지정한 WBP 를 띄운다. 로드 실패 시 C++ 기본 트리로 폴백.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BG|UI")
	TSoftClassPtr<UBGSessionMenuWidget> MenuWidgetClass =
		TSoftClassPtr<UBGSessionMenuWidget>(FSoftObjectPath(TEXT("/Game/UI/Session/WBP_BGSessionMenu.WBP_BGSessionMenu_C")));

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);


	UPROPERTY(Transient)
	TObjectPtr<UBGSessionMenuWidget> Menu;

	bool bOpen = false;

	FDelegateHandle PostLoadMapHandle;
	FTimerHandle RestoreTimer;
};

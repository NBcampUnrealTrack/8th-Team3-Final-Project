// 메인메뉴 컨트롤러. 메뉴 위젯을 띄우고 입력을 UI 로 넘긴다.
// 메뉴 항목의 동작은 위젯이 갖는다 — 여기는 수명만 관리한다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BGMainMenuPlayerController.generated.h"

class UBGMainMenuWidget;

UCLASS(Config = Game)
class BG_API ABGMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABGMainMenuPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 메뉴 위젯 BP. DefaultGame.ini 에서 지정한다. 없으면 경고만 남기고 띄우지 않는다.
	UPROPERTY(EditDefaultsOnly, Config, Category = "BG|UI")
	TSoftClassPtr<UBGMainMenuWidget> MenuWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UBGMainMenuWidget> MenuWidget;
};

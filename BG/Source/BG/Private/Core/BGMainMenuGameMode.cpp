#include "Core/BGMainMenuGameMode.h"

#include "Core/BGMainMenuPlayerController.h"

ABGMainMenuGameMode::ABGMainMenuGameMode()
{
	// 폰을 만들지 않는다 — 메뉴는 UI 만 띄운다.
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ABGMainMenuPlayerController::StaticClass();
}

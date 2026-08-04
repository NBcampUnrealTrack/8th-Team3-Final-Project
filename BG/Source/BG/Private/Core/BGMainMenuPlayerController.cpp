#include "Core/BGMainMenuPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Debug/BGNetLog.h"
#include "UI/Menu/BGMainMenuWidget.h"

ABGMainMenuPlayerController::ABGMainMenuPlayerController()
{
	bShowMouseCursor = true;
}

void ABGMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	UClass* WidgetClass = MenuWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogBGNet, Warning, TEXT("[MainMenu] MenuWidgetClass 미지정/로드 실패(%s) — 메뉴를 띄우지 않습니다."),
			*MenuWidgetClass.ToString());
		return;
	}

	MenuWidget = CreateWidget<UBGMainMenuWidget>(this, WidgetClass);
	if (!MenuWidget)
	{
		UE_LOG(LogBGNet, Error, TEXT("[MainMenu] 메뉴 위젯 생성 실패."));
		return;
	}

	MenuWidget->AddToViewport();

	// UIOnly 로 두면 뷰포트가 게임 입력을 통째로 무시하고, 그 설정이 트래블 뒤에도 남아
	// 게임 맵에서 F9(BG_UI) 같은 키가 죽는다. 메뉴 클릭에는 GameAndUI 로 충분하다.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	UE_LOG(LogBGNet, Log, TEXT("[MainMenu] 메뉴 열림."));
}

void ABGMainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

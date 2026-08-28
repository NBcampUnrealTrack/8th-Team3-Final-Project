#include "UI/Menu/BGMainMenuWidget.h"

#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Core/BGGameInstance.h"
#include "Debug/BGNetLog.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/Session/BGSessionMenuController.h"

void UBGMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_PvE)      { Btn_PvE->OnClicked.AddDynamic(this, &UBGMainMenuWidget::HandlePvE); }
	if (Btn_PvP)      { Btn_PvP->OnClicked.AddDynamic(this, &UBGMainMenuWidget::HandlePvP); }
	if (Btn_Host)     { Btn_Host->OnClicked.AddDynamic(this, &UBGMainMenuWidget::HandleHost); }
	if (Btn_Join)     { Btn_Join->OnClicked.AddDynamic(this, &UBGMainMenuWidget::HandleJoin); }
	if (Btn_Settings) { Btn_Settings->OnClicked.AddDynamic(this, &UBGMainMenuWidget::HandleSettings); }
	if (Btn_Quit)     { Btn_Quit->OnClicked.AddDynamic(this, &UBGMainMenuWidget::HandleQuit); }
	if (Btn_Back)     { Btn_Back->OnClicked.AddDynamic(this, &UBGMainMenuWidget::HandleBack); }

	SetMultiplayerSubmenuVisible(false);
}

UBGGameInstance* UBGMainMenuWidget::GetBGGameInstance() const
{
	return GetGameInstance<UBGGameInstance>();
}

void UBGMainMenuWidget::SetMultiplayerSubmenuVisible(bool bVisible)
{
	if (Switcher_Menu)
	{
		Switcher_Menu->SetActiveWidgetIndex(bVisible ? 1 : 0);
	}
}

void UBGMainMenuWidget::HandlePvE()
{
	if (UBGGameInstance* GI = GetBGGameInstance())
	{
		UE_LOG(LogBGNet, Log, TEXT("[MainMenu] PvE 시작."));
		GI->StartPvE();
	}
}

void UBGMainMenuWidget::HandlePvP()
{
	SetMultiplayerSubmenuVisible(true);
}

void UBGMainMenuWidget::HandleBack()
{
	SetMultiplayerSubmenuVisible(false);
}

void UBGMainMenuWidget::HandleHost()
{
	if (UBGGameInstance* GI = GetBGGameInstance())
	{
		UE_LOG(LogBGNet, Log, TEXT("[MainMenu] 방 생성 요청."));
		GI->HostSession();

		// 방 참가와 달리 호스트는 세션 메뉴를 거치지 않는다 — 열어 두지 않으면
		// 게임 맵 도착 후 준비/시작 UI 가 없는 채로 남는다.
		if (UBGSessionMenuController* Menu = GI->GetSubsystem<UBGSessionMenuController>())
		{
			Menu->Open();
		}
	}
}

void UBGMainMenuWidget::HandleJoin()
{
	// 방 목록·코드 입력은 기존 세션 메뉴가 갖고 있다 — 그것을 연다.
	if (UBGSessionMenuController* Menu = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UBGSessionMenuController>() : nullptr)
	{
		UE_LOG(LogBGNet, Log, TEXT("[MainMenu] 방 참가 — 세션 메뉴 열기."));
		Menu->Open();
	}
}

void UBGMainMenuWidget::HandleSettings()
{
	// TODO: 환경설정 미구현.
	UE_LOG(LogBGNet, Warning, TEXT("[MainMenu] 환경설정은 아직 구현되지 않았습니다."));
}

void UBGMainMenuWidget::HandleQuit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

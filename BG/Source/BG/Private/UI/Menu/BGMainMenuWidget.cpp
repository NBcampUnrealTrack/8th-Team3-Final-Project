#include "UI/Menu/BGMainMenuWidget.h"

#include "Audio/BGAudioSubsystem.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Core/BGGameInstance.h"
#include "UI/Widget/BGSystemMenuWidget.h"
#include "Debug/BGNetLog.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/Session/BGSessionMenuController.h"

void UBGMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->BindDefaultButtonsInWidget(this);
	}

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
	if (!SettingsWidgetClass)
	{
		UE_LOG(LogBGNet, Warning,
			TEXT("[MainMenu] SettingsWidgetClass 미지정 — WBP_BGMainMenu 의 BG|UI 에서 WBP_BGSystemMenu 를 고를 것."));
		return;
	}

	if (!SettingsWidgetInstance)
	{
		SettingsWidgetInstance = CreateWidget<UBGSystemMenuWidget>(GetOwningPlayer(), SettingsWidgetClass);
		if (!SettingsWidgetInstance)
		{
			return;
		}

		// 메뉴 위에 떠야 한다. 메인메뉴 자체는 AddToViewport() 기본값(0)으로 올라간다.
		SettingsWidgetInstance->AddToViewport(200);

		// 여기엔 HUD 가 없다 — 오른쪽 위 토글 버튼과 메인메뉴 복귀 버튼은 감춘다.
		SettingsWidgetInstance->SetStandaloneMode(true);
	}

	SettingsWidgetInstance->OpenSettings();
}

void UBGMainMenuWidget::HandleQuit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

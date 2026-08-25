// TRPauseMenuWidget.cpp

#include "UI/TRPauseMenuWidget.h"

#include "CommonInputModeTypes.h"
#include "GameFramework/PlayerController.h"
#include "UI/Settings/TRSettingsCommonButton.h"
#include "UI/Settings/TRSettingsWidget.h"
#include "UI/TRMenuLeave.h"
#include "UI/TRPlayerHUD.h"

UTRPauseMenuWidget::UTRPauseMenuWidget()
{
	bAutoActivate = true;
	bIsBackHandler = true;
	bAutoRestoreFocus = true;
}

void UTRPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Btn_Resume->OnClicked().RemoveAll(this);
	Btn_Resume->OnClicked().AddUObject(this, &UTRPauseMenuWidget::HandleResumeClicked);
	Btn_Settings->OnClicked().RemoveAll(this);
	Btn_Settings->OnClicked().AddUObject(this, &UTRPauseMenuWidget::HandleSettingsClicked);
	Btn_Quit->OnClicked().RemoveAll(this);
	Btn_Quit->OnClicked().AddUObject(this, &UTRPauseMenuWidget::HandleQuitClicked);

	if (APlayerController* PC = GetOwningPlayer())
	{
		ATRPlayerHUD::PushPopupInputMode(PC, this);

		// 커서만 꺼내면 마우스가 카메라까지 돌려 화면이 휙휙 튄다.
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
		bInputSuppressed = true;
	}
}

void UTRPauseMenuWidget::NativeDestruct()
{
	CloseSettings();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (bInputSuppressed)
		{
			PC->SetIgnoreLookInput(false);
			PC->SetIgnoreMoveInput(false);
			bInputSuppressed = false;
		}

		ATRPlayerHUD::PopPopupInputMode(PC);
	}

	Super::NativeDestruct();
}

// ESC. 설정이 떠 있으면 그쪽이 먼저 받아 가므로 여기까지 오지 않는다.
bool UTRPauseMenuWidget::NativeOnHandleBackAction()
{
	RequestClose();
	return true;
}

UWidget* UTRPauseMenuWidget::NativeGetDesiredFocusTarget() const
{
	return Btn_Resume;
}

TOptional<FUIInputConfig> UTRPauseMenuWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock, false);
}

void UTRPauseMenuWidget::HandleResumeClicked()
{
	RequestClose();
}

void UTRPauseMenuWidget::HandleSettingsClicked()
{
	if (SettingsPopup)
	{
		return;
	}

	TSubclassOf<UTRSettingsWidget> Class = SettingsWidgetClass.LoadSynchronous();
	if (!ensureMsgf(Class, TEXT("WBP 의 SettingsWidgetClass 가 비었거나 로드에 실패했습니다: %s"),
		*SettingsWidgetClass.ToString()))
	{
		return;
	}

	SettingsPopup = CreateWidget<UTRSettingsWidget>(GetOwningPlayer(), Class);
	if (!ensure(SettingsPopup))
	{
		return;
	}

	// 설정은 스스로 비활성화만 한다. AddToViewport 안에서 곧 활성화되므로 먼저 걸어 둔다.
	SettingsPopup->OnDeactivated().AddWeakLambda(this, [this]()
	{
		CloseSettings();
	});

	// 겹쳐 보이지 않게 접는다. 띄우기 전에 기억해야 즉시 닫혀도 되돌릴 값이 남는다.
	VisibilityBeforeSettings = GetVisibility();
	SetVisibility(ESlateVisibility::Collapsed);

	SettingsPopup->AddToViewport(210);
	SettingsPopup->ActivateWidget();
}

void UTRPauseMenuWidget::CloseSettings()
{
	if (!SettingsPopup)
	{
		return;
	}

	// RemoveFromParent 가 OnDeactivated 를 태워 이 함수로 다시 들어온다. 먼저 비운다.
	UTRSettingsWidget* Popup = SettingsPopup;
	SettingsPopup = nullptr;
	Popup->RemoveFromParent();

	SetVisibility(VisibilityBeforeSettings);

	// 포커스는 설정 쪽으로 넘어가 있다. 되찾지 않으면 키보드, 게임패드가 먹통이 된다.
	RequestRefreshFocus();
}

void UTRPauseMenuWidget::HandleQuitClicked()
{
	RequestClose();

	// 인게임 이탈이므로 싱글이어도 맵을 다시 열어 메뉴 상태로 돌아간다.
	TRMenuLeave::LeaveSessionAndReturnToMenu(this, /*bReloadWhenStandalone=*/true);
}

void UTRPauseMenuWidget::RequestClose()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		// HUD 가 다시 스폰됐으면 이 위젯을 들고 있지 않다. 그때 넘겨 버리면 아무도 안 닫는다.
		ATRPlayerHUD* HUD = PC->GetHUD<ATRPlayerHUD>();
		if (IsValid(HUD) && HUD->IsPauseMenuOpen())
		{
			HUD->ClosePauseMenu();
			return;
		}
	}

	// HUD 를 못 찾으면 직접 내린다. 안 그러면 화면에 남아 입력 억제를 쥔 채 ESC 를 막는다.
	if (IsInViewport())
	{
		RemoveFromParent();
	}
}

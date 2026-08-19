// TRMainMenuWidget.cpp

#include "UI/TRMainMenuWidget.h"
#include "CommonButtonBase.h"
#include "Engine/GameInstance.h"
#include "UI/Session/TRSessionMenuController.h"

void UTRMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(Btn_FindGame))
	{
		Btn_FindGame->OnClicked().RemoveAll(this);
		Btn_FindGame->OnClicked().AddUObject(this, &UTRMainMenuWidget::HandleFindGameClicked);
	}
}

void UTRMainMenuWidget::HandleFindGameClicked()
{
	UGameInstance* GameInstance = GetGameInstance();
	UTRSessionMenuController* Controller = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UTRSessionMenuController>()
		: nullptr;

	if (IsValid(Controller))
	{
		Controller->Open();
	}
}

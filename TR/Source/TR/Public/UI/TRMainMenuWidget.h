// TRMainMenuWidget.h

#pragma once

#include "CommonUserWidget.h"
#include "TRMainMenuWidget.generated.h"

class UCommonButtonBase;

UCLASS(Abstract)
class TR_API UTRMainMenuWidget : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_FindGame;

private:
	void HandleFindGameClicked();
};

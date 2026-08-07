// TRPlayerHUDWidget.h

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TRPlayerHUDWidget.generated.h"

class UWidget;

UCLASS(Abstract)
class TR_API UTRPlayerHUDWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetInteractionVisible(bool bVisible);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerInteraction;
};

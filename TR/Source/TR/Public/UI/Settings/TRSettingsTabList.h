// TRSettingsTabList.h

#pragma once

#include "CommonTabListWidgetBase.h"
#include "TRSettingsTabList.generated.h"

class UHorizontalBox;

UCLASS(Abstract)
class TR_API UTRSettingsTabList : public UCommonTabListWidgetBase
{
	GENERATED_BODY()

public:
	UTRSettingsTabList(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton) override;
	virtual void HandleTabRemoval_Implementation(FName TabNameID, UCommonButtonBase* TabButton) override;

	// 탭 버튼이 들어가는 자리. 버튼은 런타임 생성이라 Designer 에는 안 보인다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> TabButtonBox;
};

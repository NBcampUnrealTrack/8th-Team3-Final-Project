// TRSettingsTabList.cpp

#include "UI/Settings/TRSettingsTabList.h"

#include "CommonButtonBase.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

UTRSettingsTabList::UTRSettingsTabList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 이 값이 꺼져 있으면 베이스가 입력을 아예 듣지 않아 탭 순환 액션이 죽는다.
	// 실제로 좌우로 넘기려면 Next/PreviousTabInputActionData 행도 채워야 한다.
	bAutoListenForInput = true;
}

void UTRSettingsTabList::HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
	if (!IsValid(TabButtonBox) || !IsValid(TabButton))
	{
		return;
	}

	if (UHorizontalBoxSlot* BoxSlot = TabButtonBox->AddChildToHorizontalBox(TabButton))
	{
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BoxSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}
}

void UTRSettingsTabList::HandleTabRemoval_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
	if (IsValid(TabButton))
	{
		TabButton->RemoveFromParent();
	}
}

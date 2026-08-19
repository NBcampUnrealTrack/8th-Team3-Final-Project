// TRNameplateWidget.cpp

#include "UI/TRNameplateWidget.h"
#include "Components/TextBlock.h"

void UTRNameplateWidget::SetPlayerName(const FText& InName)
{
	if (CachedName.EqualTo(InName))
	{
		return;
	}

	CachedName = InName;

	if (Text_Name)
	{
		Text_Name->SetText(InName);
	}

	OnPlayerNameChanged(InName);
}

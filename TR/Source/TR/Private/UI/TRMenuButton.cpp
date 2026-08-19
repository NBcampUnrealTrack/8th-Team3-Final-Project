// TRMenuButton.cpp

#include "UI/TRMenuButton.h"
#include "Components/TextBlock.h"

void UTRMenuButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(Txt_Label))
	{
		Txt_Label->SetText(LabelText);
	}

	ApplyLabelColor();
}

void UTRMenuButton::NativeOnHovered()
{
	Super::NativeOnHovered();

	ApplyLabelColor();
}

void UTRMenuButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	ApplyLabelColor();
}

void UTRMenuButton::ApplyLabelColor()
{
	if (!IsValid(Txt_Label))
	{
		return;
	}

	// Super 가 호버 이벤트를 걸러 낼 수 있으므로 실제 상태를 다시 읽는다.
	Txt_Label->SetColorAndOpacity(FSlateColor(IsHovered() ? LabelHoveredColor : LabelColor));
}

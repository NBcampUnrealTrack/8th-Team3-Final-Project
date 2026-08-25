// TRMenuButton.cpp

#include "UI/TRMenuButton.h"
#include "Components/TextBlock.h"

void UTRMenuButton::SetLabelText(const FText& InLabelText)
{
	LabelText = InLabelText;

	if (IsValid(Txt_Label))
	{
		Txt_Label->SetText(LabelText);
	}
}

void UTRMenuButton::SetLocked(bool bInLocked)
{
	SetIsLocked(bInLocked);

	// 색은 여기서 직접 맞춘다.
	ApplyLabelColor();
}

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

	// 잠긴 버튼은 호버해도 눌리지 않으므로 잠금색이 우선이다.
	if (GetLocked())
	{
		Txt_Label->SetColorAndOpacity(FSlateColor(LabelLockedColor));
		return;
	}

	// Super 가 호버 이벤트를 걸러 낼 수 있으므로 실제 상태를 다시 읽는다.
	Txt_Label->SetColorAndOpacity(FSlateColor(IsHovered() ? LabelHoveredColor : LabelColor));
}

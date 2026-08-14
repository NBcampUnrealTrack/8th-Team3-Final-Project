// TRConsoleButton.cpp

#include "UI/TRConsoleButton.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UTRConsoleButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetStateText(FText::GetEmpty());
	SetProgress(-1.f);
}

void UTRConsoleButton::SetStateText(const FText& StateText)
{
	if (!IsValid(Txt_Label))
	{
		return;
	}

	Txt_Label->SetText(StateText.IsEmpty()
		? LabelText
		: FText::FromString(FString::Printf(TEXT("%s\n%s"), *LabelText.ToString(), *StateText.ToString())));
}

void UTRConsoleButton::SetProgress(float Progress01)
{
	if (!IsValid(PB_Progress))
	{
		return;
	}

	const bool bVisible = Progress01 >= 0.f;
	PB_Progress->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (bVisible)
	{
		PB_Progress->SetPercent(Progress01);
	}
}

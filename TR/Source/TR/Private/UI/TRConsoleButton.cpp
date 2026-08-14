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
	// 첫 호출은 빈 텍스트라도 반영해야 디자이너에 적어 둔 제목이 올라온다.
	if (!IsValid(Txt_Label) || (bStateTextApplied && StateText.EqualTo(LastStateText)))
	{
		return;
	}
	LastStateText = StateText;
	bStateTextApplied = true;

	Txt_Label->SetText(StateText.IsEmpty()
		? LabelText
		: FText::FromString(FString::Printf(TEXT("%s\n%s"), *LabelText.ToString(), *StateText.ToString())));
}

void UTRConsoleButton::SetProgress(float Progress01)
{
	if (!IsValid(PB_Progress) || Progress01 == LastProgress01)
	{
		return;
	}

	const bool bWasVisible = LastProgress01 >= 0.f;
	const bool bVisible = Progress01 >= 0.f;
	LastProgress01 = Progress01;

	if (bVisible != bWasVisible)
	{
		PB_Progress->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bVisible)
	{
		PB_Progress->SetPercent(Progress01);
	}
}

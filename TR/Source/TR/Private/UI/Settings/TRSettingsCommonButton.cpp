// TRSettingsCommonButton.cpp

#include "UI/Settings/TRSettingsCommonButton.h"

#include "CommonTextBlock.h"

void UTRSettingsCommonButton::SetButtonText(const FText& InText)
{
	Label = InText;
	if (ButtonText)
	{
		ButtonText->SetText(Label);
	}
}

// Designer 미리보기와 런타임 모두에 문구 반영
void UTRSettingsCommonButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (ButtonText)
	{
		ButtonText->SetText(Label);
	}
}

void UTRSettingsCommonButton::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	if (!ButtonText)
	{
		return;
	}

	// 스타일을 못 고르는 상태도 있다. 그때는 위젯에 걸린 스타일을 그대로 둔다.
	if (const TSubclassOf<UCommonTextStyle> TextStyle = GetCurrentTextStyleClass())
	{
		ButtonText->SetStyle(TextStyle);
	}
}

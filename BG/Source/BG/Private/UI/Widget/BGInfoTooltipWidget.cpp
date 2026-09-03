#include "UI/Widget/BGInfoTooltipWidget.h"

#include "Components/TextBlock.h"

void UBGInfoTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//	SetInfo 가 위젯이 만들어지기 전에 불릴 수 있다 — 그때 넣어 둔 값을 여기서 반영한다.
	RefreshVisual();
}

void UBGInfoTooltipWidget::SetInfo(const FText& InTitle, const FText& InBody)
{
	Title = InTitle;
	Body = InBody;

	RefreshVisual();
	OnInfoChanged(Title, Body);
}

void UBGInfoTooltipWidget::RefreshVisual()
{
	if (Text_Title)
	{
		//	비면 줄째로 접는다 — 빈 줄만큼 툴팁이 커지지 않게.
		Text_Title->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Text_Title->SetText(Title);
	}

	if (Text_Body)
	{
		Text_Body->SetVisibility(Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Text_Body->SetText(Body);
	}
}

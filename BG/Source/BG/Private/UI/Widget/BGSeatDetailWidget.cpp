#include "UI/Widget/BGSeatDetailWidget.h"

#include "Components/TextBlock.h"

void UBGSeatDetailWidget::SetSeatInfo(const FBGSeatView& InView)
{
	View = InView;

	if (Text_Job)       { Text_Job->SetText(GetJobText()); }
	if (Text_Role)      { Text_Role->SetText(GetRoleText()); }
	if (Text_Equipment) { Text_Equipment->SetText(GetEquipmentLine()); }

	//	손패는 판이 시작돼야 값이 생긴다. 그 전에는 줄째로 감춘다.
	if (Text_Hand)
	{
		const bool bShow = ShouldShowStats();

		Text_Hand->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShow)
		{
			Text_Hand->SetText(GetHandText());
		}
	}

	OnSeatDetailChanged(View);
}

FText UBGSeatDetailWidget::GetHealthText() const
{
	if (!ShouldShowStats())
	{
		return FText::GetEmpty();
	}

	return FText::Format(NSLOCTEXT("BG", "SeatDetailHealth", "체력 {0} / {1}"), View.Health, View.MaxHealth);
}

FText UBGSeatDetailWidget::GetHandText() const
{
	return FText::Format(NSLOCTEXT("BG", "SeatDetailHand", "손패 {0}"), View.HandCount);
}

FText UBGSeatDetailWidget::GetRoleText() const
{
	//	숨겨진 것을 빈칸으로 두면 "숨겨져 있다" 는 사실 자체가 안 보인다.
	if (View.RoleName.IsEmpty())
	{
		return NSLOCTEXT("BG", "SeatDetailHiddenRole", "역할 : ???");
	}

	return FText::Format(NSLOCTEXT("BG", "SeatDetailRole", "역할 : {0}"), View.RoleName);
}

FText UBGSeatDetailWidget::GetEquipmentLine() const
{
	if (View.Equipment.IsEmpty())
	{
		return NSLOCTEXT("BG", "SeatDetailNoEquipment", "장비 없음");
	}

	return FText::Join(NSLOCTEXT("BG", "SeatDetailEquipSeparator", ", "), View.GetEquipmentNames());
}

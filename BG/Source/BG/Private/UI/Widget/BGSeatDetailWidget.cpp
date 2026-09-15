#include "UI/Widget/BGSeatDetailWidget.h"

#include "Components/Image.h"
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

	RefreshIcons();

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

void UBGSeatDetailWidget::RefreshIcons()
{
	//	태그를 못 찾으면(배정 전이거나 역할이 숨겨져 있으면) WBP 가 깔아 둔 범용 아이콘으로
	//	되돌린다. 감추면 32px 자리가 빠져 그 줄만 텍스트 높이로 쪼그라든다.
	struct FIconSlot
	{
		UImage* Image;
		const TMap<FGameplayTag, TObjectPtr<UTexture2D>>& Table;
		FGameplayTag Tag;
		TObjectPtr<UTexture2D>& Fallback;
	};
	const FIconSlot Slots[] = {
		{ Img_JobIcon,  JobIcons,  View.JobTag,  JobIconDefault  },
		{ Img_RoleIcon, RoleIcons, View.RoleTag, RoleIconDefault },
	};

	for (const FIconSlot& IconSlot : Slots)
	{
		if (!IconSlot.Image)
		{
			continue;
		}

		if (!IconSlot.Fallback)
		{
			IconSlot.Fallback = Cast<UTexture2D>(IconSlot.Image->GetBrush().GetResourceObject());
		}

		const TObjectPtr<UTexture2D>* Found = IconSlot.Tag.IsValid() ? IconSlot.Table.Find(IconSlot.Tag) : nullptr;
		UTexture2D* Icon = Found ? Found->Get() : IconSlot.Fallback.Get();

		//	범용 아이콘조차 없으면 흰 사각형이 남으므로 그때만 감춘다.
		IconSlot.Image->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (Icon)
		{
			//	bMatchSize=false — 크기는 WBP 의 슬롯이 정한다.
			IconSlot.Image->SetBrushFromTexture(Icon, /*bMatchSize=*/ false);
		}
	}
}

FText UBGSeatDetailWidget::GetRoleText() const
{
	//	숨겨진 것을 빈칸으로 두면 "숨겨져 있다" 는 사실 자체가 안 보인다.
	if (View.RoleName.IsEmpty())
	{
		return NSLOCTEXT("BG", "SeatDetailHiddenRole", "???");
	}

	//	라벨은 붙이지 않는다 — 어느 줄인지는 옆 아이콘이 말해 준다.
	return View.RoleName;
}

FText UBGSeatDetailWidget::GetEquipmentLine() const
{
	if (View.Equipment.IsEmpty())
	{
		return NSLOCTEXT("BG", "SeatDetailNoEquipment", "장비 없음");
	}

	return FText::Join(NSLOCTEXT("BG", "SeatDetailEquipSeparator", ", "), View.GetEquipmentNames());
}

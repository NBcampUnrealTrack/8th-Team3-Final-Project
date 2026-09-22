#include "UI/Widget/BGHelpRowWidget.h"

#include "Components/Image.h"

void UBGHelpRowWidget::SetIcon(UTexture2D* Icon)
{
	if (!Img_Icon)
	{
		return;
	}

	Img_Icon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Icon)
	{
		//	bMatchSize=false — 크기는 WBP 의 슬롯이 정한다. 텍스처 해상도를 따라가면
		//	직업마다 아이콘 크기가 달라진다.
		Img_Icon->SetBrushFromTexture(Icon, /*bMatchSize=*/ false);
	}
}

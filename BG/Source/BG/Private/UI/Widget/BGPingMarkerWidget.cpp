#include "UI/Widget/BGPingMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"

void UBGPingMarkerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureFallbackVisuals();
}

namespace
{
	//	캔버스 가운데에 고정 크기로 얹는다. 화면 공간 위젯이라 픽셀로 준다.
	UImage* AddCenteredImage(UWidgetTree* Tree, UCanvasPanel* Canvas, FName Name,
		UTexture2D* Texture, const FVector2D& Size, const FVector2D& Offset)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image))
		{
			Slot->SetAnchors(FAnchors(0.5f, 0.5f));
			Slot->SetAlignment(FVector2D(0.5f, 0.5f));
			Slot->SetSize(Size);
			Slot->SetPosition(Offset);
		}
		if (Texture)
		{
			Image->SetBrushFromTexture(Texture, false);
		}

		//	텍스처가 늦게 오면 브러시가 0 크기로 남는다 — 크기는 항상 코드가 정한다.
		Image->SetDesiredSizeOverride(Size);
		return Image;
	}
}

void UBGPingMarkerWidget::EnsureFallbackVisuals()
{
	if ((Image_Ring && Image_Icon) || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root_PingMarker"));
	if (!Canvas)
	{
		return;
	}
	WidgetTree->RootWidget = Canvas;

	if (!Image_Ring)
	{
		Image_Ring = AddCenteredImage(WidgetTree, Canvas, TEXT("Image_Ring"), RingTexture,
			FVector2D(64.f, 80.f), FVector2D(0.f, 0.f));
	}

	Image_Disc = AddCenteredImage(WidgetTree, Canvas, TEXT("Image_Disc"), DiscTexture,
		FVector2D(40.f, 40.f), FVector2D(0.f, -8.f));
	Image_Disc->SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.07f, 0.9f));

	if (!Image_Icon)
	{
		Image_Icon = AddCenteredImage(WidgetTree, Canvas, TEXT("Image_Icon"), nullptr,
			FVector2D(36.f, 36.f), FVector2D(0.f, -8.f));
	}
}

void UBGPingMarkerWidget::SetPing(UTexture2D* IconTexture, FLinearColor IconColor, FLinearColor RingColor)
{
	if (Image_Icon)
	{
		if (IconTexture)
		{
			Image_Icon->SetBrushFromTexture(IconTexture, false);
		}
		Image_Icon->SetColorAndOpacity(IconColor);
	}

	if (Image_Ring)
	{
		Image_Ring->SetColorAndOpacity(RingColor);
	}

	OnPingSet(IconTexture, IconColor, RingColor);
}

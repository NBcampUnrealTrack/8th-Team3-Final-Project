#include "UI/Widget/BGPingMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UBGPingMarkerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureFallbackVisuals();
}

namespace
{
	//	캔버스 가운데를 기준으로 고정 크기로 얹는다. 화면 공간 위젯이라 픽셀로 준다.
	UCanvasPanelSlot* PlaceCentered(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Size, const FVector2D& Offset)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		if (Slot)
		{
			Slot->SetAnchors(FAnchors(0.5f, 0.5f));
			Slot->SetAlignment(FVector2D(0.5f, 0.5f));
			Slot->SetSize(Size);
			Slot->SetPosition(Offset);
		}
		return Slot;
	}

	UImage* AddCenteredImage(UWidgetTree* Tree, UCanvasPanel* Canvas, FName Name,
		UTexture2D* Texture, const FVector2D& Size, const FVector2D& Offset)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		PlaceCentered(Canvas, Image, Size, Offset);
		if (Texture)
		{
			Image->SetBrushFromTexture(Texture, false);
		}

		//	텍스처가 늦게 오면 브러시가 0 크기로 남는다 — 크기는 항상 코드가 정한다.
		Image->SetDesiredSizeOverride(Size);
		return Image;
	}
}

UCanvasPanel* UBGPingMarkerWidget::FindOrCreateFallbackCanvas()
{
	if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
	{
		return RootCanvas;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_PingFallback"));
	if (!WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = Canvas;
	}
	else if (UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
	{
		RootPanel->AddChild(Canvas);
	}
	else
	{
		return nullptr;
	}
	return Canvas;
}

void UBGPingMarkerWidget::EnsureFallbackVisuals()
{
	if (!WidgetTree || (Image_Ring && Image_Body && Image_Icon && Border_NamePlate && Border_NamePlateBack && Text_Badge))
	{
		return;
	}

	//	바인딩되지 않은 시안용 자식(자리표시 글자 등)은 접는다 — 루트는 그대로 둔다.
	const TSet<UWidget*> Bound = { Image_Ring, Image_Body, Image_Icon, Border_NamePlate, Border_NamePlateBack, Text_Badge };
	WidgetTree->ForEachWidget([&Bound, this](UWidget* Widget)
	{
		if (Widget && Widget != WidgetTree->RootWidget && !Widget->IsA<UPanelWidget>() && !Bound.Contains(Widget))
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	});

	UCanvasPanel* Canvas = FindOrCreateFallbackCanvas();
	if (!Canvas)
	{
		return;
	}

	//	핀은 64x80, 머리 중심은 위에서 40%(=가운데보다 8px 위)다.
	const FVector2D PinSize(64.f, 80.f);
	const FVector2D HeadOffset(0.f, -8.f);

	if (!Image_Ring)
	{
		Image_Ring = AddCenteredImage(WidgetTree, Canvas, TEXT("Image_Ring"), RingTexture, PinSize, FVector2D::ZeroVector);
	}
	if (!Image_Body && BodyTexture)
	{
		Image_Body = AddCenteredImage(WidgetTree, Canvas, TEXT("Image_Body"), BodyTexture, PinSize, FVector2D::ZeroVector);
	}
	if (!Image_Icon)
	{
		Image_Icon = AddCenteredImage(WidgetTree, Canvas, TEXT("Image_Icon"), nullptr, FVector2D(44.f, 44.f), HeadOffset);
	}

	//	보낸 사람 이름표 — 핀 꼭대기 바로 위 가운데. 테두리(좌석 색) > 바탕(검정 80%) > 이름.
	if (!Text_Badge)
	{
		Text_Badge = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Badge"));
		FSlateFontInfo Font = Text_Badge->GetFont();
		Font.Size = 11;
		Font.TypefaceFontName = TEXT("Bold");
		Text_Badge->SetFont(Font);
		Text_Badge->SetJustification(ETextJustify::Center);
		Text_Badge->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.95f, 0.82f)));
	}
	if (!Border_NamePlateBack)
	{
		Border_NamePlateBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_NamePlateBack"));
		Border_NamePlateBack->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Border_NamePlateBack->SetPadding(FMargin(6.f, 1.f));
		Border_NamePlateBack->SetContent(Text_Badge);
	}
	if (!Border_NamePlate)
	{
		Border_NamePlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_NamePlate"));
		Border_NamePlate->SetPadding(FMargin(2.f));
		Border_NamePlate->SetContent(Border_NamePlateBack);
		if (UCanvasPanelSlot* PlateSlot = Canvas->AddChildToCanvas(Border_NamePlate))
		{
			//	아래 가운데를 핀 꼭대기(가운데보다 40 위) 바로 위에 붙이고, 폭은 이름 길이에 맞춘다.
			PlateSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PlateSlot->SetAlignment(FVector2D(0.5f, 1.f));
			PlateSlot->SetAutoSize(true);
			PlateSlot->SetPosition(FVector2D(0.f, -42.f));
		}
		Border_NamePlate->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBGPingMarkerWidget::SetPing(UTexture2D* IconTexture, FLinearColor IconColor, FLinearColor RingColor)
{
	if (Image_Icon)
	{
		SetIconTexture(IconTexture);
		Image_Icon->SetColorAndOpacity(IconColor);
	}

	SetSenderColor(RingColor);

	OnPingSet(IconTexture, IconColor, RingColor);
}

void UBGPingMarkerWidget::SetIconTexture(UTexture2D* IconTexture)
{
	if (Image_Icon && IconTexture)
	{
		Image_Icon->SetBrushFromTexture(IconTexture, false);
		Image_Icon->SetDesiredSizeOverride(FVector2D(44.f, 44.f));
	}
}

void UBGPingMarkerWidget::SetSenderName(const FString& SenderName, int32 SeatNumber, FLinearColor SenderColor)
{
	const bool bShow = SeatNumber > 0 || !SenderName.IsEmpty();
	if (Text_Badge)
	{
		FString Shown = SenderName.IsEmpty() ? FString::Printf(TEXT("P%d"), SeatNumber) : SenderName;
		if (Shown.Len() > 8)
		{
			Shown = Shown.Left(8) + TEXT("\u2026");
		}
		Text_Badge->SetText(FText::FromString(Shown));
	}
	if (Border_NamePlate)
	{
		Border_NamePlate->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	SetSenderColor(SenderColor);
}

void UBGPingMarkerWidget::SetSenderColor(FLinearColor SenderColor)
{
	if (Image_Ring)
	{
		Image_Ring->SetColorAndOpacity(SenderColor);
	}
	if (Border_NamePlate)
	{
		Border_NamePlate->SetBrushColor(SenderColor);
	}
}

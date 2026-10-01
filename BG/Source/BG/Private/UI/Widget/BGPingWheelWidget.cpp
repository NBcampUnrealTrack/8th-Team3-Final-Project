#include "UI/Widget/BGPingWheelWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/BGUIStyleSubsystem.h"

namespace
{
	const FLinearColor Gold(0.83f, 0.66f, 0.29f);
	const FLinearColor Ivory(1.f, 0.95f, 0.82f);

	UCanvasPanelSlot* PlaceWheelCentered(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Size, const FVector2D& Offset)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		if (Slot)
		{
			Slot->SetAnchors(FAnchors(0.5f, 0.5f));
			Slot->SetAlignment(FVector2D(0.5f, 0.5f));
			Slot->SetAutoSize(false);
			Slot->SetSize(Size);
			Slot->SetPosition(Offset);
		}
		return Slot;
	}

	UTextBlock* MakeLabel(UUserWidget* Owner, UWidgetTree* Tree, FName Name, int32 FontSize)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		UBGUIStyleSubsystem::ApplyDefaultTypeface(Owner, Font);
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor::Black;
		Text->SetFont(Font);
		Text->SetJustification(ETextJustify::Center);
		Text->SetColorAndOpacity(FSlateColor(Ivory));
		return Text;
	}
}

void UBGPingWheelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureFallbackVisuals();
}

FVector2D UBGPingWheelWidget::SlotOffset(EBGPingWheelDirection Direction, float Radius)
{
	switch (Direction)
	{
	case EBGPingWheelDirection::Up:    return FVector2D(0.f, -Radius);
	case EBGPingWheelDirection::Right: return FVector2D(Radius, 0.f);
	case EBGPingWheelDirection::Down:  return FVector2D(0.f, Radius);
	case EBGPingWheelDirection::Left:  return FVector2D(-Radius, 0.f);
	default:                           return FVector2D::ZeroVector;
	}
}

void UBGPingWheelWidget::EnsureFallbackVisuals()
{
	if (SlotImages.Num() > 0 || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Canvas)
	{
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root_PingWheel"));
		WidgetTree->RootWidget = Canvas;
	}
	if (!Canvas)
	{
		return;
	}

	//	디자인 시안용으로 들어 있던 자식은 접는다 — 코드 구성만 보이게 한다.
	for (UWidget* Child : Canvas->GetAllChildren())
	{
		if (Child)
		{
			Child->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	//	원판 → 선택 강조 → 칸 아이콘 → 라벨 순으로 쌓는다.
	Image_Back = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_WheelBack"));
	PlaceWheelCentered(Canvas, Image_Back, FVector2D(BackDiameter), FVector2D::ZeroVector);
	if (BackTexture)
	{
		Image_Back->SetBrushFromTexture(BackTexture, false);
	}
	else
	{
		Image_Back->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	}
	Image_Back->SetDesiredSizeOverride(FVector2D(BackDiameter));

	const float HighlightSize = IconSize * 1.15f + 20.f;
	Image_Highlight = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_WheelHighlight"));
	PlaceWheelCentered(Canvas, Image_Highlight, FVector2D(HighlightSize), FVector2D::ZeroVector);
	if (HighlightTexture)
	{
		Image_Highlight->SetBrushFromTexture(HighlightTexture, false);
	}
	Image_Highlight->SetDesiredSizeOverride(FVector2D(HighlightSize));
	Image_Highlight->SetColorAndOpacity(HighlightTexture ? Gold : FLinearColor(Gold.R, Gold.G, Gold.B, 0.3f));

	for (const FBGPingDef& Def : BGPing::GetDefaultDefs())
	{
		const uint8 Key = static_cast<uint8>(Def.WheelDirection);
		const FVector2D Offset = SlotOffset(Def.WheelDirection, SlotRadius);

		UImage* SlotImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			FName(*FString::Printf(TEXT("Image_Slot_%d"), Key)));
		PlaceWheelCentered(Canvas, SlotImage, FVector2D(IconSize), Offset);
		SlotImage->SetDesiredSizeOverride(FVector2D(IconSize));
		SlotImages.Add(Key, SlotImage);
		SlotColors.Add(Key, Def.IconColor);
		SlotLabelTexts.Add(Key, Def.Label);

		//	칸마다 아래에 고정 라벨(중앙 = 공격).
		UTextBlock* Label = MakeLabel(this, WidgetTree, FName(*FString::Printf(TEXT("Text_Slot_%d"), Key)), 13);
		PlaceWheelCentered(Canvas, Label, FVector2D(64.f, 20.f), Offset + FVector2D(0.f, IconSize * 0.5f + 10.f));
		Label->SetText(Def.Label);
		SlotLabels.Add(Key, Label);
	}

	//	현재 선택명은 원판 바깥 아래(하단 + 14).
	Text_Selection = MakeLabel(this, WidgetTree, TEXT("Text_Selection"), 14);
	PlaceWheelCentered(Canvas, Text_Selection, FVector2D(120.f, 22.f), FVector2D(0.f, BackDiameter * 0.5f + 14.f + 11.f));

	RefreshSlots();
}

void UBGPingWheelWidget::SetSlotIcon(EBGPingWheelDirection Direction, UTexture2D* IconTexture, FLinearColor IconColor, FText Label)
{
	const uint8 Key = static_cast<uint8>(Direction);
	SlotColors.Add(Key, IconColor);
	if (!Label.IsEmpty())
	{
		SlotLabelTexts.Add(Key, Label);
		if (TObjectPtr<UTextBlock>* LabelWidget = SlotLabels.Find(Key))
		{
			(*LabelWidget)->SetText(Label);
		}
	}

	if (TObjectPtr<UImage>* Found = SlotImages.Find(Key))
	{
		if (*Found && IconTexture)
		{
			(*Found)->SetBrushFromTexture(IconTexture, false);
			(*Found)->SetDesiredSizeOverride(FVector2D(IconSize));
		}
	}

	RefreshSlots();
}

void UBGPingWheelWidget::RefreshSlots()
{
	const bool bActiveEnabled = IsDirectionEnabled(Highlighted);

	for (const TPair<uint8, TObjectPtr<UImage>>& Pair : SlotImages)
	{
		UImage* SlotImage = Pair.Value;
		if (!SlotImage)
		{
			continue;
		}

		const EBGPingWheelDirection Direction = static_cast<EBGPingWheelDirection>(Pair.Key);
		const bool bEnabled = IsDirectionEnabled(Direction);
		const bool bActive = (Direction == Highlighted) && bEnabled;

		FLinearColor Color = SlotColors.FindRef(Pair.Key);
		if (!bEnabled)
		{
			//	고를 수 없는 칸은 색을 빼고 흐리게 둔다.
			const float Grey = Color.GetLuminance();
			Color = FLinearColor(Grey, Grey, Grey, 0.35f);
		}
		else if (!bActive)
		{
			Color = Color * 0.8f;
			Color.A = 1.f;
		}

		SlotImage->SetColorAndOpacity(Color);
		SlotImage->SetRenderScale(bActive ? FVector2D(1.15f, 1.15f) : FVector2D(1.f, 1.f));

		if (TObjectPtr<UTextBlock>* Label = SlotLabels.Find(Pair.Key))
		{
			(*Label)->SetColorAndOpacity(FSlateColor(bEnabled ? (bActive ? Gold : Ivory) : FLinearColor(0.5f, 0.5f, 0.5f, 0.6f)));
		}
	}

	if (Image_Highlight)
	{
		if (UCanvasPanelSlot* HighlightSlot = Cast<UCanvasPanelSlot>(Image_Highlight->Slot))
		{
			HighlightSlot->SetPosition(SlotOffset(Highlighted, SlotRadius));
		}
		Image_Highlight->SetVisibility(bActiveEnabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Text_Selection)
	{
		const FText* Name = SlotLabelTexts.Find(static_cast<uint8>(Highlighted));
		Text_Selection->SetText((bActiveEnabled && Name) ? *Name : FText::FromString(TEXT("핑 선택")));
	}
}

void UBGPingWheelWidget::Open(FVector2D ScreenPos, bool bInHasTarget)
{
	bHasTarget = bInHasTarget;
	Highlighted = EBGPingWheelDirection::Center;

	//	라벨까지 들어가는 크기. 화면 가장자리 EdgeMargin 안쪽으로 중심을 민다.
	const float Extent = (BackDiameter * 0.5f + 14.f + 22.f) * 2.f;
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	const FVector2D ViewportPx = UWidgetLayoutLibrary::GetViewportSize(this);
	const float HalfPx = (Extent * 0.5f + EdgeMargin) * Scale;
	if (ViewportPx.X > HalfPx * 2.f && ViewportPx.Y > HalfPx * 2.f)
	{
		ScreenPos.X = FMath::Clamp(ScreenPos.X, HalfPx, ViewportPx.X - HalfPx);
		ScreenPos.Y = FMath::Clamp(ScreenPos.Y, HalfPx, ViewportPx.Y - HalfPx);
	}
	OpenedScreenPos = ScreenPos;

	SetDesiredSizeInViewport(FVector2D(Extent, Extent));
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	SetPositionInViewport(ScreenPos, true);
	SetVisibility(ESlateVisibility::HitTestInvisible);

	RefreshSlots();
	OnWheelOpened(ScreenPos, bInHasTarget);
}

void UBGPingWheelWidget::SetHighlighted(EBGPingWheelDirection Direction)
{
	if (Highlighted == Direction)
	{
		return;
	}

	Highlighted = Direction;
	RefreshSlots();
	OnHighlightChanged(Direction);
}

void UBGPingWheelWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);
	OnWheelClosed();
}

bool UBGPingWheelWidget::IsDirectionEnabled(EBGPingWheelDirection Direction) const
{
	if (bHasTarget)
	{
		return true;
	}

	for (const FBGPingDef& Def : BGPing::GetDefaultDefs())
	{
		if (Def.WheelDirection == Direction)
		{
			return !Def.bNeedsTarget;
		}
	}

	return true;
}

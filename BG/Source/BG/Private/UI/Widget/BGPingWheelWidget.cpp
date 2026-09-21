#include "UI/Widget/BGPingWheelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UBGPingWheelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureFallbackVisuals();
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

	for (const FBGPingDef& Def : BGPing::GetDefaultDefs())
	{
		FVector2D Offset = FVector2D::ZeroVector;
		switch (Def.WheelDirection)
		{
		case EBGPingWheelDirection::Up:    Offset = FVector2D(0.f, -SlotRadius); break;
		case EBGPingWheelDirection::Right: Offset = FVector2D(SlotRadius, 0.f);  break;
		case EBGPingWheelDirection::Down:  Offset = FVector2D(0.f, SlotRadius);  break;
		case EBGPingWheelDirection::Left:  Offset = FVector2D(-SlotRadius, 0.f); break;
		default: break;
		}

		UImage* SlotImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			FName(*FString::Printf(TEXT("Image_Slot_%d"), static_cast<int32>(Def.WheelDirection))));
		const FVector2D SlotSize = (Def.WheelDirection == EBGPingWheelDirection::Center)
			? FVector2D(64.f, 64.f) : FVector2D(56.f, 56.f);

		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(SlotImage))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetSize(SlotSize);
			CanvasSlot->SetPosition(Offset);
		}
		SlotImage->SetDesiredSizeOverride(SlotSize);

		const uint8 Key = static_cast<uint8>(Def.WheelDirection);
		SlotImages.Add(Key, SlotImage);
		SlotColors.Add(Key, Def.IconColor);
	}

	RefreshSlots();
}

void UBGPingWheelWidget::SetSlotIcon(EBGPingWheelDirection Direction, UTexture2D* IconTexture, FLinearColor IconColor)
{
	const uint8 Key = static_cast<uint8>(Direction);
	SlotColors.Add(Key, IconColor);

	if (TObjectPtr<UImage>* Found = SlotImages.Find(Key))
	{
		if (*Found && IconTexture)
		{
			const FVector2D SlotSize = (Direction == EBGPingWheelDirection::Center)
				? FVector2D(64.f, 64.f) : FVector2D(56.f, 56.f);
			(*Found)->SetBrushFromTexture(IconTexture, false);
			(*Found)->SetDesiredSizeOverride(SlotSize);
		}
	}

	RefreshSlots();
}

void UBGPingWheelWidget::RefreshSlots()
{
	for (const TPair<uint8, TObjectPtr<UImage>>& Pair : SlotImages)
	{
		UImage* SlotImage = Pair.Value;
		if (!SlotImage)
		{
			continue;
		}

		const EBGPingWheelDirection Direction = static_cast<EBGPingWheelDirection>(Pair.Key);
		const bool bEnabled = IsDirectionEnabled(Direction);
		const bool bActive = (Direction == Highlighted);

		FLinearColor Color = SlotColors.FindRef(Pair.Key);
		if (!bEnabled)
		{
			//	고를 수 없는 칸은 색을 빼고 흐리게 둔다.
			const float Grey = Color.GetLuminance();
			Color = FLinearColor(Grey, Grey, Grey, 0.35f);
		}
		else if (!bActive)
		{
			Color = Color * 0.75f;
			Color.A = 1.f;
		}

		SlotImage->SetColorAndOpacity(Color);
		SlotImage->SetRenderScale(bActive && bEnabled ? FVector2D(1.35f, 1.35f) : FVector2D(1.f, 1.f));
	}
}

void UBGPingWheelWidget::Open(FVector2D ScreenPos, bool bInHasTarget)
{
	bHasTarget = bInHasTarget;
	Highlighted = EBGPingWheelDirection::Center;

	SetPositionInViewport(ScreenPos, false);
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

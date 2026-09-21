#include "UI/Widget/BGSeatInfoWidget.h"
#include "Components/Image.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	//	코드로 만든 텍스트 위젯은 엔진 기본 폰트(Roboto)를 물고 나온다 — WBP 쪽과 같은 폰트로 맞춘다.
	void ApplySeatInfoFont(UTextBlock* Block)
	{
		FSlateFontInfo Font = Block->GetFont();
		if (UObject* Loaded = FSoftObjectPath(TEXT("/Game/UI/Font/HeirofLightRegular_Font.HeirofLightRegular_Font")).TryLoad())
		{
			Font.FontObject = Loaded;
			Font.TypefaceFontName = TEXT("Default");
		}
		Block->SetFont(Font);
	}
}

void UBGSeatInfoWidget::SetSeatInfo(const FBGSeatView& InView)
{
	View = InView;

	//	값은 여기서 밀어넣는다. 프로퍼티 바인딩을 쓰면 매 프레임 다시 계산된다.
	if (Text_Name)
	{
		Text_Name->SetText(GetNameText());
	}

	if (Text_Health)
	{
		//	판이 시작되기 전에는 빈 텍스트가 오므로 줄째로 감춘다.
		const bool bShow = ShouldShowStats();

		Text_Health->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShow)
		{
			Text_Health->SetText(GetHealthText());
		}
	}

	RefreshHealthHearts();

	OnSeatInfoChanged(View);
}

//	WBP 가 배치를 주면 RootWidget 이 이미 차 있다 — 그때는 만들지 않고 그쪽에 맡긴다.
TSharedRef<SWidget> UBGSeatInfoWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackLayout();
	}

	return Super::RebuildWidget();
}

void UBGSeatInfoWidget::RefreshHealthHearts()
{
	UImage* const Hearts[] = { Img_Heart_1, Img_Heart_2, Img_Heart_3, Img_Heart_4, Img_Heart_5, Img_Heart_6 };
	const int32 NumHearts = UE_ARRAY_COUNT(Hearts);

	//	판 전에는 최대 체력이 0 이다 — 줄째로 접힌다.
	const int32 MaxHealth = ShouldShowStats() ? FMath::Min(View.MaxHealth, NumHearts) : 0;
	const int32 Health = FMath::Clamp(View.Health, 0, MaxHealth);

	for (int32 Index = 0; Index < NumHearts; ++Index)
	{
		UImage* Heart = Hearts[Index];
		if (!Heart)
		{
			continue;
		}

		//	최대 체력을 넘는 칸은 자리까지 접는다. 숨기기만 하면 줄이 가운데로 모이지 않는다.
		if (Index >= MaxHealth)
		{
			Heart->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		Heart->SetVisibility(ESlateVisibility::HitTestInvisible);
		Heart->SetColorAndOpacity(Index < Health ? HeartFullTint : HeartEmptyTint);
	}
}

void UBGSeatInfoWidget::BuildFallbackLayout()
{
	UOverlay*     Root   = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_Root"));
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box_Lines"));
	if (!Root || !Column)
	{
		return;
	}
	WidgetTree->RootWidget = Root;

	//	아래 정렬이라 체력 줄이 숨겨져도 머리 위 높이가 흔들리지 않는다.
	if (UOverlaySlot* PlateSlot = Cast<UOverlaySlot>(Root->AddChild(Column)))
	{
		PlateSlot->SetHorizontalAlignment(HAlign_Center);
		PlateSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	Text_Name   = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Name"));
	Text_Health = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Health"));

	//	배경판이 없으므로 그림자가 유일한 대비 수단이다.
	for (UTextBlock* Line : { Text_Name.Get(), Text_Health.Get() })
	{
		if (!Line)
		{
			continue;
		}

		ApplySeatInfoFont(Line);
		Line->SetJustification(ETextJustify::Center);
		Line->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		Line->SetShadowOffset(FVector2D(1.f, 1.f));
		Column->AddChild(Line);
	}

	if (Text_Health)
	{
		Text_Health->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.42f, 0.38f, 1.f)));
	}
}

FText UBGSeatInfoWidget::GetNameText() const
{
	if (!View.bIsAlive)
	{
		return FText::Format(NSLOCTEXT("BG", "SeatInfoDead", "{0} (사망)"), View.PlayerName);
	}
	if (View.bIsDisconnected)
	{
		return FText::Format(NSLOCTEXT("BG", "SeatInfoOffline", "{0} (접속 끊김)"), View.PlayerName);
	}
	return View.PlayerName;
}

FText UBGSeatInfoWidget::GetHealthText() const
{
	if (!ShouldShowStats())
	{
		return FText::GetEmpty();
	}

	return FText::Format(NSLOCTEXT("BG", "SeatInfoHealth", "{0} / {1}"), View.Health, View.MaxHealth);
}

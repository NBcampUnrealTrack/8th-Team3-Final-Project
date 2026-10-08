#include "UI/Widget/BGSeatInfoWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UI/BGUIStyleSubsystem.h"

namespace
{
	//	이름은 12자까지만 보여준다. 이름표 폭이 이름을 따라 끝없이 늘어나지 않게 한다.
	FText ClampDisplayName(const FText& Name)
	{
		constexpr int32 MaxNameChars = 12;

		const FString& Source = Name.ToString();
		return Source.Len() > MaxNameChars
			? FText::FromString(Source.Left(MaxNameChars) + TEXT("…"))
			: Name;
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

	if (Text_Job)       { Text_Job->SetText(GetJobText()); }
	if (Text_Role)      { Text_Role->SetText(GetRoleText()); }
	if (Text_Equipment) { Text_Equipment->SetText(GetEquipmentLine()); }

	//	손패는 판이 시작돼야 값이 생긴다. 그 전에는 숫자만 감춘다(아이콘은 줄 높이를 지키려 남긴다).
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

	OnSeatInfoChanged(View);
}

void UBGSeatInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//	처음에는 접어 둔다. 커서가 올라가야 펼쳐진다. 처음 상태는 애니메이션 없이 바로 입힌다.
	DetailExpandAlpha = bDetailExpanded ? 1.f : 0.f;
	ApplyDetailExpand();
}

void UBGSeatInfoWidget::SetDetailExpanded(bool bExpanded)
{
	bDetailExpanded = bExpanded;

	//	감쌀 상자가 없으면 높이를 못 늘린다 — 바로 바꾼다.
	if (!Size_Detail || DetailExpandSeconds <= 0.f)
	{
		DetailExpandAlpha = bExpanded ? 1.f : 0.f;
		ApplyDetailExpand();
	}
}

void UBGSeatInfoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float Target = bDetailExpanded ? 1.f : 0.f;
	if (DetailExpandAlpha == Target)
	{
		return;
	}

	DetailExpandAlpha = DetailExpandSeconds > 0.f
		? FMath::FInterpConstantTo(DetailExpandAlpha, Target, InDeltaTime, 1.f / DetailExpandSeconds)
		: Target;

	ApplyDetailExpand();
}

void UBGSeatInfoWidget::ApplyDetailExpand()
{
	if (Panel_Detail)
	{
		//	접혀도 Collapsed 가 아니라 Hidden 으로 둔다 — 폭 계산에는 남아 있어야 펼칠 때 패널 폭이 튀지 않는다.
		//	높이는 Size_Detail 이 0 으로 막는다. 그 상자가 없을 때만 자리째 뺀다.
		const ESlateVisibility Folded = Size_Detail ? ESlateVisibility::Hidden : ESlateVisibility::Collapsed;
		Panel_Detail->SetVisibility(DetailExpandAlpha > 0.f ? ESlateVisibility::HitTestInvisible : Folded);
		Panel_Detail->SetRenderOpacity(DetailExpandAlpha);
	}

	if (!Size_Detail)
	{
		return;
	}

	//	다 펼쳐졌으면 높이를 풀어 둔다. 펼친 채로 장비가 늘어도 상자가 따라 커진다.
	if (DetailExpandAlpha >= 1.f)
	{
		Size_Detail->ClearHeightOverride();
		return;
	}

	float FullHeight = 0.f;
	if (Panel_Detail && DetailExpandAlpha > 0.f)
	{
		Panel_Detail->ForceLayoutPrepass();
		FullHeight = Panel_Detail->GetDesiredSize().Y;

		//	상자 안쪽 여백도 높이에 들어간다. 빼면 마지막 줄이 잘린 채 멈췄다가 툭 늘어난다.
		if (const USizeBoxSlot* DetailSlot = Cast<USizeBoxSlot>(Panel_Detail->Slot))
		{
			FullHeight += DetailSlot->GetPadding().GetTotalSpaceAlong<Orient_Vertical>();
		}
	}

	//	처음엔 빠르게 열리고 끝에서 잦아든다.
	Size_Detail->SetHeightOverride(FullHeight * FMath::InterpEaseOut(0.f, 1.f, DetailExpandAlpha, 2.f));
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

void UBGSeatInfoWidget::RefreshIcons()
{
	//	태그를 못 찾으면(배정 전이거나 역할이 숨겨져 있으면) WBP 가 깔아 둔 범용 아이콘으로
	//	되돌린다. 감추면 아이콘 자리가 빠져 그 줄만 텍스트 높이로 쪼그라든다.
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

		FSlateFontInfo Font = Line->GetFont();
		UBGUIStyleSubsystem::ApplyDefaultTypeface(this, Font);
		Line->SetFont(Font);
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
	//	자르는 것은 이름뿐이다 — 사망·이탈 표시까지 잘리면 안 된다.
	const FText Name = ClampDisplayName(View.PlayerName);

	if (!View.bIsAlive)
	{
		return FText::Format(NSLOCTEXT("BG", "SeatInfoDead", "{0} (사망)"), Name);
	}
	if (View.bIsDisconnected)
	{
		return FText::Format(NSLOCTEXT("BG", "SeatInfoOffline", "{0} (접속 끊김)"), Name);
	}
	return Name;
}

FText UBGSeatInfoWidget::GetHealthText() const
{
	if (!ShouldShowStats())
	{
		return FText::GetEmpty();
	}

	return FText::Format(NSLOCTEXT("BG", "SeatInfoHealth", "{0} / {1}"), View.Health, View.MaxHealth);
}

FText UBGSeatInfoWidget::GetHandText() const
{
	return FText::AsNumber(View.HandCount);
}

FText UBGSeatInfoWidget::GetRoleText() const
{
	//	숨겨진 것을 빈칸으로 두면 "숨겨져 있다" 는 사실 자체가 안 보인다.
	if (View.RoleName.IsEmpty())
	{
		return NSLOCTEXT("BG", "SeatInfoHiddenRole", "???");
	}

	//	라벨은 붙이지 않는다 — 어느 줄인지는 옆 아이콘이 말해 준다.
	return View.RoleName;
}

FText UBGSeatInfoWidget::GetEquipmentLine() const
{
	if (View.Equipment.IsEmpty())
	{
		return NSLOCTEXT("BG", "SeatInfoNoEquipment", "장비 없음");
	}

	return FText::Join(NSLOCTEXT("BG", "SeatInfoEquipSeparator", ", "), View.GetEquipmentNames());
}

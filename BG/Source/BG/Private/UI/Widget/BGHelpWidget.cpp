#include "UI/Widget/BGHelpWidget.h"

#include "BGGameplayTags.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Card/BGGameDataTypes.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Engine/DataTable.h"
#include "UI/View/BGInfoTooltipRow.h"
#include "UI/Widget/BGHelpRowWidget.h"

namespace
{
	// 구분선이 화면에서 차지해야 할 픽셀 두께. 설정 창과 같은 값이다.
	constexpr float HelpDividerPixels = 2.f;

	// 읽는 순서는 진영 순서다. 태그를 코드가 갖고 있으므로 DT 의 행 순서에 맡기지 않는다.
	const FGameplayTag* RoleOrder()
	{
		static const FGameplayTag Tags[] = {
			BG::Role::Sheriff.GetTag(), BG::Role::Deputy.GetTag(),
			BG::Role::Outlaw.GetTag(),  BG::Role::Renegade.GetTag() };
		return Tags;
	}
	constexpr int32 RoleCount = 4;

	// 직업 이름. DT 의 DisplayName 이 우선이고, 비어 있으면 태그의 마지막 마디를 쓴다
	// (Character.Job.Mage → "Mage"). 폴백이 보기 나빠도 그대로 둔다 — DT 를 채우라는 신호다.
	FText JobTitle(const FBGCharacterRow& Row)
	{
		if (!Row.DisplayName.IsEmpty())
		{
			return Row.DisplayName;
		}

		const FString Tag = Row.CharacterTag.ToString();
		FString Last;
		return Tag.Split(TEXT("."), nullptr, &Last, ESearchCase::IgnoreCase, ESearchDir::FromEnd)
			? FText::FromString(Last)
			: FText::FromString(Tag);
	}

	UTexture2D* FindIcon(const TMap<FGameplayTag, TObjectPtr<UTexture2D>>& Table, const FGameplayTag& Tag)
	{
		const TObjectPtr<UTexture2D>* Found = Tag.IsValid() ? Table.Find(Tag) : nullptr;
		return Found ? Found->Get() : nullptr;
	}
}

void UBGHelpWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HelpButton)  { HelpButton->OnClicked.AddUniqueDynamic(this, &UBGHelpWidget::HandleHelpClicked); }
	if (CloseButton) { CloseButton->OnClicked.AddUniqueDynamic(this, &UBGHelpWidget::HandleCloseClicked); }

	Dividers = { Image_Div1, Image_Div2 };
	RefreshDividerThickness();

	// 문구는 판이 돌아가는 동안 바뀌지 않는다 — 한 번 만들어 두고 열 때는 보이기만 한다.
	BuildRows();

	// 패널은 버튼을 눌러야 열린다.
	if (HelpPanel)
	{
		HelpPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBGHelpWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 창 크기·해상도가 바뀌면 DPI 스케일도 바뀐다. 값이 달라졌을 때만 다시 맞춘다.
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (!FMath::IsNearlyEqual(Scale, LastViewportScale))
	{
		RefreshDividerThickness();
	}
}

void UBGHelpWidget::RefreshDividerThickness()
{
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (Scale <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	LastViewportScale = Scale;

	//	스케일이 곱해진 뒤 정확히 HelpDividerPixels 가 되도록 미리 나눠 둔다.
	const float Thickness = HelpDividerPixels / Scale;
	for (UImage* Divider : Dividers)
	{
		if (!Divider)
		{
			continue;
		}
		FSlateBrush Brush = Divider->GetBrush();
		Brush.ImageSize.Y = Thickness;
		Divider->SetBrush(Brush);
	}
}

void UBGHelpWidget::BuildRows()
{
	if (!RowClass)
	{
		return;
	}

	if (VBox_Jobs)
	{
		//	NativeConstruct 는 위젯이 다시 뷰포트에 붙을 때마다 불릴 수 있다 — 행이 쌓이지 않게 비운다.
		VBox_Jobs->ClearChildren();
		if (VBox_Jobs2)
		{
			VBox_Jobs2->ClearChildren();
		}

		if (CharacterTable)
		{
			//	둘째 열이 있으면 반씩 나눠 담는다. 홀수면 앞 열이 한 줄 더 가져간다.
			//	열이 하나뿐이면(WBP 에 VBox_Jobs2 가 없으면) 전부 앞 열로 간다.
			const int32 FirstColumnCount = VBox_Jobs2
				? FMath::DivideAndRoundUp(CharacterTable->GetRowMap().Num(), 2)
				: MAX_int32;

			int32 Index = 0;
			for (const TPair<FName, uint8*>& Pair : CharacterTable->GetRowMap())
			{
				UVerticalBox* Column = (Index++ < FirstColumnCount) ? VBox_Jobs.Get() : VBox_Jobs2.Get();
				if (const FBGCharacterRow* Row = reinterpret_cast<const FBGCharacterRow*>(Pair.Value))
				{
					AddRow(*Column, JobTitle(*Row), Row->Description, FindIcon(JobIcons, Row->CharacterTag));
				}
			}
		}
	}

	if (VBox_Roles)
	{
		VBox_Roles->ClearChildren();

		for (int32 Index = 0; Index < RoleCount; ++Index)
		{
			const FGameplayTag& Tag = RoleOrder()[Index];

			//	Row 이름은 태그 문자열 그대로다. 없으면 그 역할만 빠진다 — 경고는 끈다.
			const FBGInfoTooltipRow* Row = InfoTable
				? InfoTable->FindRow<FBGInfoTooltipRow>(FName(*Tag.ToString()), TEXT("BGHelp Role"), /*bWarnIfMissing=*/ false)
				: nullptr;

			if (Row)
			{
				AddRow(*VBox_Roles, Row->Title, Row->Body, FindIcon(RoleIcons, Tag));
			}
		}
	}
}

void UBGHelpWidget::AddRow(UVerticalBox& Box, const FText& Title, const FText& Body, UTexture2D* Icon)
{
	UBGHelpRowWidget* Row = CreateWidget<UBGHelpRowWidget>(this, RowClass);
	if (!Row)
	{
		return;
	}

	Row->SetInfo(Title, Body);
	Row->SetIcon(Icon);

	//	행 사이 여백은 WBP_BGHelpRow 가 자기 패딩으로 갖는다 — 슬롯을 여기서 만지지 않는다.
	Box.AddChildToVerticalBox(Row);
}

void UBGHelpWidget::OpenHelp()
{
	if (HelpPanel)
	{
		HelpPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UBGHelpWidget::CloseHelp()
{
	if (HelpPanel)
	{
		HelpPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBGHelpWidget::HandleHelpClicked()
{
	const bool bOpen = HelpPanel && HelpPanel->GetVisibility() != ESlateVisibility::Collapsed;
	if (bOpen)
	{
		CloseHelp();
	}
	else
	{
		OpenHelp();
	}
}

void UBGHelpWidget::HandleCloseClicked()
{
	CloseHelp();
}

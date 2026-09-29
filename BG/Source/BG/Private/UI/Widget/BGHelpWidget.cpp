#include "UI/Widget/BGHelpWidget.h"

#include "BGGameplayTags.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Card/BGGameDataTypes.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
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
	if (Tab_Jobs)    { Tab_Jobs->OnClicked.AddUniqueDynamic(this, &UBGHelpWidget::HandleJobsTabClicked); }
	if (Tab_Roles)   { Tab_Roles->OnClicked.AddUniqueDynamic(this, &UBGHelpWidget::HandleRolesTabClicked); }
	if (Tab_Equips)  { Tab_Equips->OnClicked.AddUniqueDynamic(this, &UBGHelpWidget::HandleEquipsTabClicked); }

	Dividers = { Image_Div1, Image_Div2 };
	RefreshDividerThickness();

	// 문구는 판이 돌아가는 동안 바뀌지 않는다 — 한 번 만들어 두고 열 때는 보이기만 한다.
	BuildRows();
	SelectTab(0);

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

	if (Grid_Jobs)
	{
		//	NativeConstruct 는 위젯이 다시 뷰포트에 붙을 때마다 불릴 수 있다 — 행이 쌓이지 않게 비운다.
		Grid_Jobs->ClearChildren();

		int32 Count = 0;
		if (CharacterTable)
		{
			for (const TPair<FName, uint8*>& Pair : CharacterTable->GetRowMap())
			{
				if (const FBGCharacterRow* Row = reinterpret_cast<const FBGCharacterRow*>(Pair.Value))
				{
					AddRow(*Grid_Jobs, Count++, JobTitle(*Row), Row->Description, FindIcon(JobIcons, Row->CharacterTag));
				}
			}
		}
		PadFirstRow(*Grid_Jobs, Count);
	}

	if (Grid_Roles)
	{
		Grid_Roles->ClearChildren();

		int32 Count = 0;
		for (int32 Index = 0; Index < RoleCount; ++Index)
		{
			const FGameplayTag& Tag = RoleOrder()[Index];

			//	Row 이름은 태그 문자열 그대로다. 없으면 그 역할만 빠진다 — 경고는 끈다.
			const FBGInfoTooltipRow* Row = InfoTable
				? InfoTable->FindRow<FBGInfoTooltipRow>(FName(*Tag.ToString()), TEXT("BGHelp Role"), /*bWarnIfMissing=*/ false)
				: nullptr;

			if (Row)
			{
				AddRow(*Grid_Roles, Count++, Row->Title, Row->Body, FindIcon(RoleIcons, Tag));
			}
		}
		PadFirstRow(*Grid_Roles, Count);
	}

	if (Grid_Equips)
	{
		Grid_Equips->ClearChildren();

		int32 Count = 0;
		if (CardTable)
		{
			for (const TPair<FName, uint8*>& Pair : CardTable->GetRowMap())
			{
				const FBGCardRow* Row = reinterpret_cast<const FBGCardRow*>(Pair.Value);
				if (Row && Row->TypeTag == BG::Card::Type::Blue.GetTag())
				{
					AddRow(*Grid_Equips, Count++, Row->DisplayName, Row->Description, FindIcon(EquipIcons, Row->CardIdTag));
				}
			}
		}
		PadFirstRow(*Grid_Equips, Count);
	}
}

void UBGHelpWidget::AddRow(UUniformGridPanel& Grid, int32 Index, const FText& Title, const FText& Body, UTexture2D* Icon)
{
	UBGHelpRowWidget* Row = CreateWidget<UBGHelpRowWidget>(this, RowClass);
	if (!Row)
	{
		return;
	}

	Row->SetInfo(Title, Body);
	Row->SetIcon(Icon);

	//	행 사이 세로 여백은 WBP_BGHelpRow 가 자기 패딩으로, 열 사이 여백은 격자의 SlotPadding 이 갖는다.
	const int32 ColumnCount = FMath::Max(1, Columns);
	Grid.AddChildToUniformGrid(Row, Index / ColumnCount, Index % ColumnCount);
}

void UBGHelpWidget::PadFirstRow(UUniformGridPanel& Grid, int32 Count)
{
	for (int32 Index = Count; Index < Columns; ++Index)
	{
		Grid.AddChildToUniformGrid(NewObject<USpacer>(this), 0, Index);
	}
}

void UBGHelpWidget::SelectTab(int32 Index)
{
	if (Scroll_Body)
	{
		Scroll_Body->ScrollToStart();
	}

	UButton* Buttons[] = { Tab_Jobs, Tab_Roles, Tab_Equips };
	UTextBlock* Texts[] = { Tab_Jobs_Text, Tab_Roles_Text, Tab_Equips_Text };
	UUniformGridPanel* Pages[] = { Grid_Jobs, Grid_Roles, Grid_Equips };
	for (int32 Tab = 0; Tab < UE_ARRAY_COUNT(Buttons); ++Tab)
	{
		const bool bActive = (Tab == Index);
		if (Pages[Tab])
		{
			//	Collapsed 가 아니라 Hidden — 자리를 남겨야 Overlay 가 가장 큰 탭 크기를 유지한다.
			Pages[Tab]->SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
		}
		if (Buttons[Tab])
		{
			Buttons[Tab]->SetBackgroundColor(bActive ? FLinearColor::White : InactiveTabTint);
		}
		if (Texts[Tab])
		{
			Texts[Tab]->SetColorAndOpacity(bActive ? ActiveTabColor : InactiveTabColor);
		}
	}
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

void UBGHelpWidget::HandleJobsTabClicked()
{
	SelectTab(0);
}

void UBGHelpWidget::HandleRolesTabClicked()
{
	SelectTab(1);
}

void UBGHelpWidget::HandleEquipsTabClicked()
{
	SelectTab(2);
}

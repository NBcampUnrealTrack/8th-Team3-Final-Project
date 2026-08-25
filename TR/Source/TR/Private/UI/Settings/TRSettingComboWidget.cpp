// TRSettingComboWidget.cpp

#include "UI/Settings/TRSettingComboWidget.h"

#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"

// ComboBox 선택 변경 이벤트 바인딩
void UTRSettingComboWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ValueComboBox->OnSelectionChanged.AddUniqueDynamic(this, &UTRSettingComboWidget::HandleSelectionChanged);
}

// 설정 메타데이터와 선택지를 ComboBox에 적용
void UTRSettingComboWidget::InitializeSetting(const ETRSettingId InSettingId, const FText& InDisplayName,
	const TArray<FString>& InOptions, const FString& SelectedOption)
{
	SettingId = InSettingId;
	SettingNameText->SetText(InDisplayName);
	TGuardValue<bool> Guard(bSynchronizing, true);
	ValueComboBox->ClearOptions();
	for (const FString& Option : InOptions)
	{
		ValueComboBox->AddOption(Option);
	}
	if (!SelectedOption.IsEmpty() && ValueComboBox->FindOptionIndex(SelectedOption) == INDEX_NONE)
	{
		ValueComboBox->AddOption(SelectedOption);
	}
	ValueComboBox->SetSelectedOption(SelectedOption);
}

// 외부 설정값 반영 중 변경 이벤트 전파 방지
void UTRSettingComboWidget::SetSelectedOption(const FString& Option)
{
	TGuardValue<bool> Guard(bSynchronizing, true);
	if (!Option.IsEmpty() && ValueComboBox->FindOptionIndex(Option) == INDEX_NONE)
	{
		ValueComboBox->AddOption(Option);
	}
	ValueComboBox->SetSelectedOption(Option);
}

FString UTRSettingComboWidget::GetSelectedOption() const
{
	return ValueComboBox->GetSelectedOption();
}

// 실제 사용자 선택만 메인 설정 위젯으로 전파
void UTRSettingComboWidget::HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!bSynchronizing && SelectionType != ESelectInfo::Direct && !SelectedItem.IsEmpty())
	{
		OnSettingOptionChanged.Broadcast(SettingId, SelectedItem);
	}
}

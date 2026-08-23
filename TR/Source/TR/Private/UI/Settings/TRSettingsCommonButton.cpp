// TRSettingsCommonButton.cpp

#include "UI/Settings/TRSettingsCommonButton.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"

// 표시 문구 캐시와 WBP TextBlock 동시 갱신
void UTRSettingsCommonButton::SetButtonText(const FText& InText)
{
	Label = InText;
	if (ButtonText)
	{
		ButtonText->SetText(Label);
	}
}

// CommonUI 선택 상태와 Designer 배경색 동기화
void UTRSettingsCommonButton::SetSettingsSelected(const bool bInSelected)
{
	SetIsSelected(bInSelected, false);
	if (ButtonBackground)
	{
		ButtonBackground->SetBrushColor(bInSelected ? SelectedColor : NormalColor);
	}
}

// 상태별 배경색을 교체하고 현재 상태 즉시 반영
void UTRSettingsCommonButton::SetButtonColors(const FLinearColor& InNormal, const FLinearColor& InHovered, const FLinearColor& InSelected)
{
	NormalColor = InNormal;
	HoveredColor = InHovered;
	SelectedColor = InSelected;
	RefreshVisualState();
}

// Designer 미리보기와 런타임 모두에 문구 및 색상 반영
void UTRSettingsCommonButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (ButtonText)
	{
		ButtonText->SetText(Label);
	}
	RefreshVisualState();
}

// 선택 상태 진입 시 선택 색상 적용
void UTRSettingsCommonButton::NativeOnSelected(const bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	if (ButtonBackground)
	{
		ButtonBackground->SetBrushColor(SelectedColor);
	}
}

// 선택 해제 시 기본 색상 복원
void UTRSettingsCommonButton::NativeOnDeselected(const bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	if (ButtonBackground)
	{
		ButtonBackground->SetBrushColor(NormalColor);
	}
}

// 선택되지 않은 버튼에만 호버 색상 적용
void UTRSettingsCommonButton::NativeOnHovered()
{
	Super::NativeOnHovered();
	if (ButtonBackground && !GetSelected())
	{
		ButtonBackground->SetBrushColor(HoveredColor);
	}
}

// 호버 종료 후 선택 여부에 맞는 색상 복원
void UTRSettingsCommonButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	if (ButtonBackground)
	{
		ButtonBackground->SetBrushColor(GetSelected() ? SelectedColor : NormalColor);
	}
}

// 현재 CommonButton 상태를 Designer 배경에 반영
void UTRSettingsCommonButton::RefreshVisualState()
{
	if (!ButtonBackground)
	{
		return;
	}
	ButtonBackground->SetBrushColor(GetSelected() ? SelectedColor : (IsHovered() ? HoveredColor : NormalColor));
}

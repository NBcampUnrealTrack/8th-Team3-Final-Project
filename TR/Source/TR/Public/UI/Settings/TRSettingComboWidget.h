// TRSettingComboWidget.h

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Settings/TRSettingsTypes.h"
#include "TRSettingComboWidget.generated.h"

class UComboBoxString;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTRSettingOptionChanged, ETRSettingId, SettingId, const FString&, SelectedOption);

// 선택형 설정을 ComboBox로 표시하는 Designer 기반 행 위젯
UCLASS(Blueprintable)
class TR_API UTRSettingComboWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// 설정 ID, 표시 이름, 선택지와 현재 선택값 초기화
	void InitializeSetting(ETRSettingId InSettingId, const FText& InDisplayName, const TArray<FString>& InOptions, const FString& SelectedOption);

	// 외부 설정값 반영
	void SetSelectedOption(const FString& Option);
	FString GetSelectedOption() const;

	UPROPERTY(BlueprintAssignable, Category = "TR|Settings")
	FTRSettingOptionChanged OnSettingOptionChanged;

protected:
	//--- WBP 바인딩 위젯 ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> SettingNameText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UComboBoxString> ValueComboBox;

private:
	// 사용자 선택만 메인 설정 위젯으로 전파
	UFUNCTION()
	void HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	//--- 설정값 캐시 ---
	ETRSettingId SettingId = ETRSettingId::None;
	bool bSynchronizing = false;
};

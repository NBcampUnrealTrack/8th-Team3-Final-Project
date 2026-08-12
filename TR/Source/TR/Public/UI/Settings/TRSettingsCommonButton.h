// TRSettingsCommonButton.h

#pragma once

#include "CommonButtonBase.h"
#include "TRSettingsCommonButton.generated.h"

class UBorder;
class UTextBlock;

// 설정 화면에서 공통으로 사용하는 Designer 기반 CommonUI 버튼
UCLASS(Blueprintable)
class TR_API UTRSettingsCommonButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	// 버튼 문구와 선택 상태를 코드에서 갱신
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|CommonUI")
	void SetButtonText(const FText& InText);
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|CommonUI")
	void SetSettingsSelected(bool bInSelected);

	// WBP에서 지정한 상태별 색상 갱신
	void SetButtonColors(const FLinearColor& InNormal, const FLinearColor& InHovered, const FLinearColor& InSelected);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;

	//--- WBP 바인딩 위젯 ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> ButtonBackground;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ButtonText;

	//--- Designer 편집 속성 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Settings|CommonUI")
	FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Settings|CommonUI")
	FLinearColor NormalColor = FLinearColor(0.12f, 0.14f, 0.18f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Settings|CommonUI")
	FLinearColor HoveredColor = FLinearColor(0.18f, 0.30f, 0.38f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Settings|CommonUI")
	FLinearColor SelectedColor = FLinearColor(0.05f, 0.42f, 0.62f, 1.0f);

private:
	// 현재 선택/호버 상태에 맞춰 배경색 갱신
	void RefreshVisualState();
};

// TRSettingsCommonButton.h

#pragma once

#include "CommonButtonBase.h"
#include "TRSettingsCommonButton.generated.h"

class UCommonTextBlock;

// 설정 화면 공통 버튼.
UCLASS(Blueprintable)
class TR_API UTRSettingsCommonButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TR|Settings|CommonUI")
	void SetButtonText(const FText& InText);

protected:
	virtual void NativePreConstruct() override;

	// CommonButtonBase 는 스타일을 골라 통지만 한다. 위젯에 물리는 것은 이쪽 몫이다.
	virtual void NativeOnCurrentTextStyleChanged() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ButtonText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Settings|CommonUI")
	FText Label;
};

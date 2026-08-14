// TRConsoleButton.h

#pragma once

#include "CommonButtonBase.h"
#include "TRConsoleButton.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS(Abstract)
class TR_API UTRConsoleButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void SetStateText(const FText& StateText);

	// 진행률을 버튼 자리에서 바로 보여 준다. 음수를 넣으면 감춘다.
	void SetProgress(float Progress01);

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button", meta = (MultiLine = true))
	FText LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_Progress;
};

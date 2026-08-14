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

private:
	// UTextBlock::SetText 과 UProgressBar::SetPercent 은 값이 같아도 Slate 를 무효화한다.
	// 매 프레임 갱신되는 위젯이라 마지막 값을 들고 있다가 바뀔 때만 넘긴다.
	FText LastStateText;
	bool bStateTextApplied = false;

	float LastProgress01 = MAX_flt;
};

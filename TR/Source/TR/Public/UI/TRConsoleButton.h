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

	// 진행률을 버튼 자리에서 바로 보여 준다.
	void SetProgress(float Progress01);

	// 할당된 파츠의 체력 비율.
	void SetHealth01(float Health01);

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button", meta = (MultiLine = true))
	FText LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_Progress;

	// 버튼 아래에 따로 놓이는 체력바. 진행률(PB_Progress)과는 별개
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_Health;

private:
	// UTextBlock::SetText 과 UProgressBar::SetPercent 은 값이 같아도 Slate 를 무효화한다.
	// 매 프레임 갱신되는 위젯이라 마지막 값을 들고 있다가 바뀔 때만 넘긴다.
	FText LastStateText;
	bool bStateTextApplied = false;

	float LastProgress01 = MAX_flt;

	float LastHealth01 = MAX_flt;
};

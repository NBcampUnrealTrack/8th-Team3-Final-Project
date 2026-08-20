// TRMenuButton.h

#pragma once

#include "CommonButtonBase.h"
#include "TRMenuButton.generated.h"

class UTextBlock;

UCLASS(Abstract)
class TR_API UTRMenuButton : public UCommonButtonBase
{
	GENERATED_BODY()

protected:
	virtual void NativePreConstruct() override;

	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FText LabelText;

	UPROPERTY(EditAnywhere, Category = "Button")
	FLinearColor LabelColor = FLinearColor(0.92f, 0.94f, 0.95f, 1.f);

	// 호버시 배경이 밝아지므로 글자는 어두워져야 읽힌다.
	UPROPERTY(EditAnywhere, Category = "Button")
	FLinearColor LabelHoveredColor = FLinearColor(0.05f, 0.06f, 0.07f, 1.f);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Label;

private:
	void ApplyLabelColor();
};

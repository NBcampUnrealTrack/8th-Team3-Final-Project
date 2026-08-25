// TRMenuButton.h

#pragma once

#include "CommonButtonBase.h"
#include "TRMenuButton.generated.h"

class UTextBlock;

UCLASS(Abstract)
class TR_API UTRMenuButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	// 준비/준비 취소처럼 상태에 따라 글자가 바뀌는 버튼을 위해 열어 둔다.
	void SetLabelText(const FText& InLabelText);

	// SetIsEnabled와 달리 Slate의 disabled 효과를 타지 않아 배경은 그대로 두고 글자만 흐려진다.
	void SetLocked(bool bInLocked);

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

	UPROPERTY(EditAnywhere, Category = "Button")
	FLinearColor LabelLockedColor = FLinearColor(0.35f, 0.37f, 0.39f, 1.f);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Label;

private:
	void ApplyLabelColor();
};

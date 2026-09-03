// BANG! 정보 툴팁 — 턴 패널의 직업/장비/손패/역할 줄에 커서를 올리면 뜬다.
//
// C++ 는 제목과 본문만 넘기고 그리는 것은 WBP 가 정한다(이름표·상세 패널과 같은 원칙).
// 화면 위치는 슬레이트가 잡는다 — 커서를 따라다니고 화면 밖으로 나가지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BGInfoTooltipWidget.generated.h"

class UTextBlock;

/**
 * WBP 서브클래스에 아래 이름의 위젯이 있으면 자동으로 채워진다:
 *   Text_Title (TextBlock, 선택)
 *   Text_Body  (TextBlock, 선택 — 여러 줄이 오므로 Auto Wrap 을 켤 것)
 * 둘 다 없으면 OnInfoChanged 에서 직접 그린다.
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGInfoTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 툴팁이 뜨기 직전에 불린다. 같은 위젯을 다시 채워 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Info")
	void SetInfo(const FText& InTitle, const FText& InBody);

	UFUNCTION(BlueprintPure, Category = "BG|Info")
	const FText& GetTitle() const { return Title; }

	UFUNCTION(BlueprintPure, Category = "BG|Info")
	const FText& GetBody() const { return Body; }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Body;

	/** 아이콘·색처럼 텍스트 말고 더 할 것이 있으면 여기서 한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Info")
	void OnInfoChanged(const FText& InTitle, const FText& InBody);

private:
	void RefreshVisual();

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Body;
};

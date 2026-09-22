// BANG! 도움말 한 줄 — 아이콘 + 제목 + 설명.
//
// 제목·본문을 채우고 빈 줄을 접는 일은 UBGInfoTooltipWidget 이 이미 한다. 그대로
// 물려받고 여기서 더하는 것은 왼쪽 아이콘 하나뿐이다.
//
// WBP 서브클래스에 아래 이름으로 위젯을 올리면 C++ 이 이름으로 찾아 붙는다.
//   Text_Title Text_Body   (부모가 채운다 — 본문은 Auto Wrap 을 켤 것)
//   Img_Icon               (Image, 선택 — 아이콘이 없으면 접힌다)

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/BGInfoTooltipWidget.h"
#include "BGHelpRowWidget.generated.h"

class UImage;
class UTexture2D;

UCLASS(Abstract, Blueprintable)
class BG_API UBGHelpRowWidget : public UBGInfoTooltipWidget
{
	GENERATED_BODY()

public:
	/** 아이콘을 꽂는다. nullptr 이면 이미지를 접는다 — 빈 흰 사각형을 남기지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Help")
	void SetIcon(UTexture2D* Icon);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Icon;
};

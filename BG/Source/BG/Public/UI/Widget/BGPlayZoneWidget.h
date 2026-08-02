// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGCardView.h"
#include "BGPlayZoneWidget.generated.h"

class UBorder;

/**
 * 화면 중앙의 카드 사용 존. 손패에서 끌어온 카드를 여기에 드롭하면 사용 요청이 발생한다.
 *
 * WBP 서브클래스에 아래가 있어야 한다:
 *   Border_Highlight (Border, 선택 - 드래그가 올라왔을 때 강조 표시)
 *
 * ⚠️ 드롭 이벤트를 받으려면 이 위젯의 Visibility 가 Visible 이어야 한다.
 *    (HitTestInvisible / SelfHitTestInvisible 은 드롭을 받지 못한다.)
 *    Visible 이면 이 영역이 아래쪽 위젯의 마우스 입력을 가로챈다는 뜻이므로,
 *    중앙에 다른 클릭 대상을 놓지 말 것.
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGPlayZoneWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 카드가 이 존에 드롭되었다.
	 * 보통은 여기에 바인딩할 필요가 없다 — 드롭은 자동으로 원본 핸드 위젯의
	 * OnCardPlayRequested 로 전달된다. 양쪽에 모두 바인딩하면 두 번 호출된다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "BG|PlayZone")
	FOnBGCardPlayRequested OnCardDropped;

protected:
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Highlight;

	UPROPERTY(EditAnywhere, Category = "BG|PlayZone")
	FLinearColor IdleColor = FLinearColor(1.f, 1.f, 1.f, 0.08f);

	UPROPERTY(EditAnywhere, Category = "BG|PlayZone")
	FLinearColor HoverColor = FLinearColor(0.2f, 0.9f, 0.3f, 0.35f);

	/** 사용할 수 없는 카드(bPlayable == false)가 올라왔을 때의 색. */
	UPROPERTY(EditAnywhere, Category = "BG|PlayZone")
	FLinearColor RejectColor = FLinearColor(0.9f, 0.2f, 0.2f, 0.35f);

private:
	void SetHighlight(const FLinearColor& Color);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "UI/View/BGCardView.h"
#include "BGCardDragDropOperation.generated.h"

class UBGCardWidget;
class UBGHandWidget;

/** 손패에서 끌고 있는 카드 1장을 나르는 드래그 페이로드. */
UCLASS()
class BG_API UBGCardDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "BG|Drag")
	FBGCardView CardView;

	/** 드래그를 시작한 원본 카드 위젯. 드래그가 끝나면 하이라이트를 복구해야 한다. */
	UPROPERTY()
	TWeakObjectPtr<UBGCardWidget> SourceCardWidget;

	/** 사용 요청을 최종적으로 브로드캐스트할 핸드 위젯. */
	UPROPERTY()
	TWeakObjectPtr<UBGHandWidget> SourceHandWidget;

	// Drop / DragCancelled 은 BlueprintNativeEvent 이므로 _Implementation 을 재정의한다.
	virtual void Drop_Implementation(const FPointerEvent& PointerEvent) override;
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;

private:
	/** 성공/취소 어느 쪽이든 원본 위젯의 드래그 표시를 되돌린다. */
	void RestoreSourceWidget();
};

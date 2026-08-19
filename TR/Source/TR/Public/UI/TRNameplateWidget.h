// TRNameplateWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TRNameplateWidget.generated.h"

class UTextBlock;

/**
 * 머리 위에 뜨는 닉네임 표시. WBP 에 Text_Name 을 만들면 자동으로 연결된다.
 * 없으면 OnPlayerNameChanged 로만 알린다.
 */
UCLASS(Blueprintable)
class TR_API UTRNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 이름을 바꾼다. 같은 값이면 아무것도 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "TR|UI")
	void SetPlayerName(const FText& InName);

protected:
	/** WBP 가 글자를 직접 그릴 때 쓴다. Text_Name 이 있으면 그쪽도 함께 갱신된다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TR|UI")
	void OnPlayerNameChanged(const FText& InName);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

private:
	FText CachedName;
};

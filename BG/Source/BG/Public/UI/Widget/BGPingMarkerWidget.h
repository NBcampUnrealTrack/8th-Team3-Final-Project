// 핑 마커 — 지도 핀 모양. 가운데 종류 아이콘, 테두리 링은 보낸 사람 색.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BGPingMarkerWidget.generated.h"

class UImage;
class UTexture2D;

UCLASS(Abstract, Blueprintable)
class BG_API UBGPingMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시 내용을 한 번에 넣는다. 아이콘이 없으면 색만 적용된다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void SetPing(UTexture2D* IconTexture, FLinearColor IconColor, FLinearColor RingColor);

protected:
	virtual void NativeOnInitialized() override;

	/** BindWidget 이 없는 WBP 를 위한 최소 구성(링+아이콘)을 코드로 만든다. */
	void EnsureFallbackVisuals();

	/** 핀 실루엣(테두리 = 보낸 사람 색). 비어 있으면 그 겹은 생략된다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	TObjectPtr<UTexture2D> RingTexture;

	/** 아이콘 뒤에 깔 어두운 원. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	TObjectPtr<UTexture2D> DiscTexture;

	UPROPERTY()
	TObjectPtr<UImage> Image_Disc;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	/** 보낸 사람 색이 들어가는 테두리 링. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Ring;

	/** 페이드·튀어오름 같은 연출은 BP 에서 여기에 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Ping")
	void OnPingSet(UTexture2D* IconTexture, const FLinearColor& IconColor, const FLinearColor& RingColor);
};

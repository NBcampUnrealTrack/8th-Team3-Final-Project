// 핑 마커 — 지도 핀 모양. 핀 위에 보낸 사람 이름표(좌석 색 테두리), 안쪽은 어두운 몸체 + 종류 아이콘.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BGPingMarkerWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;

UCLASS(Abstract, Blueprintable)
class BG_API UBGPingMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시 내용을 한 번에 넣는다. 아이콘이 없으면 색만 적용된다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void SetPing(UTexture2D* IconTexture, FLinearColor IconColor, FLinearColor RingColor);

	/** 선로딩이 끝난 뒤 아이콘만 채운다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void SetIconTexture(UTexture2D* IconTexture);

	/** 보낸 사람 이름표. 이름이 비면 "P{좌석번호}", 8자를 넘으면 말줄임. 좌석번호 0 이하면 숨긴다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void SetSenderName(const FString& SenderName, int32 SeatNumber, FLinearColor SenderColor);

	/** 보낸 사람 색만 다시 칠한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void SetSenderColor(FLinearColor SenderColor);

protected:
	virtual void NativeOnInitialized() override;

	/** WBP 에 없는 요소만 코드로 만든다. 디자이너가 둔 위젯·루트는 그대로 둔다. */
	void EnsureFallbackVisuals();

	/** 핀 실루엣(바깥 띠 = 보낸 사람 색). 비어 있으면 그 겹은 생략된다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	TObjectPtr<UTexture2D> RingTexture;

	/** 예전 원형 배지용. 이름표로 바뀌어 지금은 쓰지 않는다(WBP 기본값 호환). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	TObjectPtr<UTexture2D> DiscTexture;

	/** 핀 안쪽 몸체(어두운 바탕 + 청동 테두리가 구워진 텍스처, 틴트하지 않는다). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	TObjectPtr<UTexture2D> BodyTexture;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Body;

	/** 이름표 테두리(보낸 사람 좌석 색, 안쪽 여백 2px 가 곧 테두리). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_NamePlate;

	/** 이름표 바탕(검정 80%). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_NamePlateBack;

	/** 이름 글자. 이름 길이에 맞춰 폭이 늘어난다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Badge;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	/** 보낸 사람 색이 들어가는 바깥 띠. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Ring;

	/** 페이드·튀어오름 같은 연출은 BP 에서 여기에 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Ping")
	void OnPingSet(UTexture2D* IconTexture, const FLinearColor& IconColor, const FLinearColor& RingColor);

private:
	/** 폴백 요소를 얹을 캔버스. 루트가 캔버스면 그대로, 다른 패널이면 그 안에 하나 만든다. */
	UCanvasPanel* FindOrCreateFallbackCanvas();
};

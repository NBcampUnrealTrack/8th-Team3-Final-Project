// 핑 휠 — 4방향 + 중앙(공격). 원판·라벨·선택 강조는 코드 폴백, 열고 닫는 판단은 BGPingComponent.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ping/BGPingTypes.h"
#include "BGPingWheelWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

UCLASS(Abstract, Blueprintable)
class BG_API UBGPingWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 누른 화면 좌표에 휠을 연다. 화면 가장자리에 걸리면 안쪽으로 밀어 연다. 대상이 없으면 좌석 필수 칸은 회색이 된다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void Open(FVector2D ScreenPos, bool bInHasTarget);

	/** 실제로 연 중심(가장자리 보정 후). 방향 판정의 원점이다. */
	UFUNCTION(BlueprintPure, Category = "BG|Ping")
	FVector2D GetOpenedScreenPos() const { return OpenedScreenPos; }

	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void SetHighlighted(EBGPingWheelDirection Direction);

	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void Close();

	/** 이 칸을 고를 수 있는가. 대상 필수 핑은 대상이 없으면 false. */
	UFUNCTION(BlueprintPure, Category = "BG|Ping")
	bool IsDirectionEnabled(EBGPingWheelDirection Direction) const;

	UFUNCTION(BlueprintPure, Category = "BG|Ping")
	EBGPingWheelDirection GetHighlighted() const { return Highlighted; }

	UFUNCTION(BlueprintPure, Category = "BG|Ping")
	bool HasTarget() const { return bHasTarget; }

	/** 칸 하나의 아이콘·색·라벨을 채운다. 발신 컴포넌트가 DT 값을 밀어 넣는다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void SetSlotIcon(EBGPingWheelDirection Direction, UTexture2D* IconTexture, FLinearColor IconColor, FText Label);

protected:
	virtual void NativeOnInitialized() override;

	/** BindWidget 이 없는 WBP 를 위한 구성(원판 + 중앙 + 4방향 + 라벨 + 선택명)을 코드로 만든다. */
	void EnsureFallbackVisuals();

	/** 칸별 색·크기·강조를 현재 상태(하이라이트·대상 유무)에 맞춘다. */
	void RefreshSlots();

	/** 원판(검정 80% + 금색 2px 테두리가 구워진 텍스처). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	TObjectPtr<UTexture2D> BackTexture;

	/** 선택 칸 뒤 강조(금색 30% 면 + 2px 외곽선, 틴트용 흰 텍스처). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	TObjectPtr<UTexture2D> HighlightTexture;

	UPROPERTY()
	TMap<uint8, TObjectPtr<UImage>> SlotImages;

	UPROPERTY()
	TMap<uint8, TObjectPtr<UTextBlock>> SlotLabels;

	UPROPERTY()
	TObjectPtr<UImage> Image_Back;

	UPROPERTY()
	TObjectPtr<UImage> Image_Highlight;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Selection;

	TMap<uint8, FLinearColor> SlotColors;
	TMap<uint8, FText> SlotLabelTexts;

	/** 칸 배치 반지름(슬레이트 단위). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	float SlotRadius = 68.f;

	/** 원판 지름. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	float BackDiameter = 188.f;

	/** 아이콘 크기(중앙 포함 통일). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	float IconSize = 32.f;

	/** 화면 가장자리에서 띄울 최소 여백. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	float EdgeMargin = 12.f;

	/** 칸 배치·아이콘·회색 처리는 BP 가 여기서 덧그릴 수 있다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Ping")
	void OnWheelOpened(FVector2D ScreenPos, bool bInHasTarget);

	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Ping")
	void OnHighlightChanged(EBGPingWheelDirection Direction);

	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Ping")
	void OnWheelClosed();

private:
	static FVector2D SlotOffset(EBGPingWheelDirection Direction, float Radius);

	EBGPingWheelDirection Highlighted = EBGPingWheelDirection::Center;

	bool bHasTarget = false;

	FVector2D OpenedScreenPos = FVector2D::ZeroVector;
};

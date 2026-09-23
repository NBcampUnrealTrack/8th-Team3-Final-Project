// 핑 휠 — 4방향 + 중앙(공격). 칸 표시와 연출은 BP, 열고 닫는 판단은 BGPingComponent.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ping/BGPingTypes.h"
#include "BGPingWheelWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class BG_API UBGPingWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 누른 화면 좌표에 휠을 연다. 대상이 없으면 좌석 필수 칸은 회색이 된다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void Open(FVector2D ScreenPos, bool bInHasTarget);

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

	/** 칸 하나의 아이콘과 색을 채운다. 발신 컴포넌트가 DT 값을 밀어 넣는다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void SetSlotIcon(EBGPingWheelDirection Direction, UTexture2D* IconTexture, FLinearColor IconColor);

protected:
	virtual void NativeOnInitialized() override;

	/** BindWidget 이 없는 WBP 를 위한 최소 구성(중앙 + 4방향)을 코드로 만든다. */
	void EnsureFallbackVisuals();

	/** 칸별 색·크기·알파를 현재 상태(하이라이트·대상 유무)에 맞춘다. */
	void RefreshSlots();

	UPROPERTY()
	TMap<uint8, TObjectPtr<class UImage>> SlotImages;

	TMap<uint8, FLinearColor> SlotColors;

	/** 칸 배치 반지름(px). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	float SlotRadius = 120.f;

	/** 칸 배치·아이콘·회색 처리는 BP 가 여기서 그린다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Ping")
	void OnWheelOpened(FVector2D ScreenPos, bool bInHasTarget);

	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Ping")
	void OnHighlightChanged(EBGPingWheelDirection Direction);

	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Ping")
	void OnWheelClosed();

private:
	EBGPingWheelDirection Highlighted = EBGPingWheelDirection::Center;

	bool bHasTarget = false;
};

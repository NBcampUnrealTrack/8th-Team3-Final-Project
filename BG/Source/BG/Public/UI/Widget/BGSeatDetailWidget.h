// BANG! 상세 정보 패널 — 커서를 올린 캐릭터 위에 뜬다.
//
// 이름표와 같은 원칙이다. C++ 는 값만 넘기고 그리는 것은 WBP 가 정한다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGSeatView.h"
#include "BGSeatDetailWidget.generated.h"

/**
 * WBP 에서 OnSeatDetailChanged 를 구현해 자기 텍스트를 채운다.
 * 역할은 공개된 것만 들어온다 — 비어 있으면 "???" 처럼 숨겨져 있다는 표시를 남길 것.
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGSeatDetailWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BG|SeatDetail")
	void SetSeatInfo(const FBGSeatView& InView);

	UFUNCTION(BlueprintPure, Category = "BG|SeatDetail")
	const FBGSeatView& GetSeatView() const { return View; }

	/** 체력 같은 판 중 정보를 보여줄 때인가. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatDetail")
	bool ShouldShowStats() const { return View.bMatchRunning && View.MaxHealth > 0; }

	// 그래프에서 구조체를 쪼개지 않도록 바로 쓸 수 있는 형태로 준다.

	UFUNCTION(BlueprintPure, Category = "BG|SeatDetail")
	FText GetNameText() const { return View.PlayerName; }

	UFUNCTION(BlueprintPure, Category = "BG|SeatDetail")
	FText GetJobText() const { return View.JobName; }

	/** "체력 4 / 5". 판이 시작되기 전에는 빈 텍스트. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatDetail")
	FText GetHealthText() const;

	/** "손패 6". */
	UFUNCTION(BlueprintPure, Category = "BG|SeatDetail")
	FText GetHandText() const;

	/** 공개된 역할만 온다. 숨겨져 있으면 "역할 : ???". */
	UFUNCTION(BlueprintPure, Category = "BG|SeatDetail")
	FText GetRoleText() const;

	/** 장비 이름을 한 줄로 이어 준다. 없으면 "장비 없음". */
	UFUNCTION(BlueprintPure, Category = "BG|SeatDetail")
	FText GetEquipmentLine() const;

protected:
	//	WBP 가 같은 이름으로 만들어 두면 자동으로 연결된다. 없어도 동작한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_Job;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_Health;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_Hand;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_Role;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_Equipment;

	/** 값이 들어왔다. 색·애니메이션처럼 텍스트 말고 더 할 것이 있으면 여기서 한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|SeatDetail")
	void OnSeatDetailChanged(const FBGSeatView& InView);

private:
	UPROPERTY()
	FBGSeatView View;
};

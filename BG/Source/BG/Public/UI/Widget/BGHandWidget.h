// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGCardView.h"
#include "BGHandWidget.generated.h"

class UBGCardWidget;
class UPanelWidget;

/**
 * 손패 위젯. FBGCardView 목록을 받아 카드 위젯을 채운다.
 *
 * WBP 서브클래스에 아래가 있어야 한다:
 *   Box_Cards (HorizontalBox 등 PanelWidget, 필수)
 * 그리고 디테일 패널에서 CardWidgetClass 에 카드 WBP 를 지정해야 한다.
 *
 * ── 게임 로직과의 연결 지점 ──
 *   들어오는 방향: SetHand()          — 로직이 손패를 밀어넣는다
 *   나가는 방향:   OnCardPlayRequested — UI 가 사용 의도를 알린다
 * 로직 파트는 이 두 개만 알면 된다.
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGHandWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 손패 전체를 교체한다. 서버에서 손패가 복제될 때마다 호출하면 된다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	void SetHand(const TArray<FBGCardView>& InCards);

	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	void ClearHand();

	/** 낙관적 제거용. 서버 확인 후 SetHand 로 덮어쓰는 것이 정석이다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	bool RemoveCardByInstanceId(int32 InstanceId);

	UFUNCTION(BlueprintPure, Category = "BG|Hand")
	int32 GetCardCount() const { return CardWidgets.Num(); }

	UFUNCTION(BlueprintPure, Category = "BG|Hand")
	UBGCardWidget* FindCardWidget(int32 InstanceId) const;

	/**
	 * 카드가 플레이 존에 드롭되어 사용 요청이 발생했다.
	 * PlayerController 가 여기에 바인딩해 ServerRequestPlayCard 를 호출하면 된다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "BG|Hand")
	FOnBGCardPlayRequested OnCardPlayRequested;

	/** 플레이 존이 드롭을 받으면 호출한다. 직접 호출로 드래그 없이 테스트할 수도 있다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	void NotifyCardPlayRequested(const FBGCardView& Card);

	/**
	 * 손패 전체의 드래그를 잠근다. 대상 좌석 선택을 기다리는 동안 쓴다.
	 *
	 * 잠금은 위젯이 아니라 손패 상태로 기억되므로, 잠긴 동안 SetHand() 로 손패가
	 * 갱신돼도 새로 만들어진 카드에 그대로 적용된다. (서버 손패 복제와 대상 선택은
	 * 동시에 일어날 수 있어서, 이게 없으면 갱신 한 번에 잠금이 풀린다.)
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	void SetInteractionLocked(bool bInLocked);

	UFUNCTION(BlueprintPure, Category = "BG|Hand")
	bool IsInteractionLocked() const { return bInteractionLocked; }

	/** 게임 로직 없이 UI 만 테스트하기 위한 더미 손패. */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Debug")
	void FillWithDebugCards(int32 Count = 5);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_Cards;

	UPROPERTY(EditAnywhere, Category = "BG|Hand")
	TSubclassOf<UBGCardWidget> CardWidgetClass;

	UPROPERTY(EditAnywhere, Category = "BG|Hand")
	FMargin CardPadding = FMargin(4.f, 0.f);

private:
	UPROPERTY()
	TArray<TObjectPtr<UBGCardWidget>> CardWidgets;

	/** 위젯이 아니라 손패에 붙는 상태. SetHand 로 카드가 새로 만들어져도 유지된다. */
	bool bInteractionLocked = false;
};

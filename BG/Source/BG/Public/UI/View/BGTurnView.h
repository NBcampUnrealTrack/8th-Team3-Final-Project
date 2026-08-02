// BANG! 턴 패널 UI 전용 뷰모델.
// FBGCardView / FBGSeatView 와 같은 원칙 — UI 는 GameState 도 PlayerState 도 모른다.

#pragma once

#include "CoreMinimal.h"
#include "BGTurnView.generated.h"

/** 턴 패널이 화면에 그릴 것 전부. */
USTRUCT(BlueprintType)
struct FBGTurnView
{
	GENERATED_BODY()

	/** 지금 내가 카드를 낼 수 있는가 (자기 턴 + Play 페이즈). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	bool bIsMyTurn = false;

	/** 지금 내가 손패를 버려야 하는가 (자기 턴 + Discard 페이즈). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	bool bIsMyDiscard = false;

	/** 판이 진행 중인가. 로비/종료 상태면 false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	bool bMatchRunning = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	int32 CurrentSeat = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	int32 MySeat = INDEX_NONE;

	/** "내 차례" / "좌석 2 진행 중" / "버릴 카드를 고르세요" 같은 안내문. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	FText StatusText;

	/** 손패 한도 표시용. 한도가 없으면 INDEX_NONE. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	int32 HandCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	int32 HandLimit = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	int32 DeckCount = 0;

	/** 이번 턴에 쓴 카드 수 / 한도 (GDD §10 — 턴 UI 필수 표시 항목). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	int32 CardsUsed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn")
	int32 CardUseLimit = 0;
};

/** 턴 종료 버튼이 눌렸다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGEndTurnRequested);

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Card/BGCardType.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameStateBase.h"
#include "BGGameState.generated.h"


/**
 * 
 */
class UBGDiceComponent;

UCLASS()
class BG_API ABGGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABGGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//	확률 판정(주사위) 전담. 어디서든 UBGDiceComponent::Get(this) 로도 얻을 수 있다.
	UFUNCTION(BlueprintPure, Category = "BG|Dice")
	UBGDiceComponent* GetDice() const { return Dice; }

	//	서버에서 덱을 구성·셔플한다. 이미 초기화됐으면 아무것도 하지 않는다(시드 보존).
	//	Seed == 0 이면 새 시드를 뽑아 쓰고, 사용한 값을 로그에 남긴다 — 재현에 필수.
	void InitializeDeck(int32 Seed = 0);

	bool DrawFromDeck(FBGCardInstance& OutCard);   // 소진 시 Discard 리셔플
	void PushToDiscard(const FBGCardInstance& Card);

	/**
	 * [미구현] 뽑기(Draw!) 판정. 수트를 쓰지 않으므로 확률 굴림으로 대체할 자리다.
	 *
	 * ⚠️ 지금은 인자를 무시하고 항상 false 를 돌려준다 — 호출하면 조용히 전부 실패한다.
	 *    감옥·다이너마이트를 넣을 때 구현한다. 반드시 Dice 의 시드된 스트림을 쓸 것
	 *    (셔플 스트림을 쓰면 판정이 덱 순서를 밀어 재현이 깨진다).
	 */
	bool PerformDrawCheck(int32 Seat, float SuccessChance);
	
	//	⚠️ 서버 전용 데이터다. 클라에서 부르면 항상 빈 배열이 나온다(덱 내용은 복제되지 않는다).
	void GetDeck(TArray<FBGCardInstance>& OutDeck) const;

	//	현재 페이즈. 클라도 읽는다 — 로비 UI 가 Phase.Play 를 보고 스스로 물러난다.
	UFUNCTION(BlueprintPure, Category = "BG|Phase")
	FGameplayTag GetPhaseTag() const { return PhaseTag; }

	//	서버 전용. 페이즈를 바꾼다 — 페이즈 전환의 유일한 입구다.
	//	같은 값이면 아무것도 하지 않는다(로그도 남기지 않는다).
	void SetPhaseTag(FGameplayTag InPhase);

	//	전원에게 복제되는 공개 카운트. 클라 UI/검증은 이 값을 본다.
	UFUNCTION(BlueprintPure, Category = "BG|Deck")
	int32 GetDeckCount() const { return DeckCount; }

	UFUNCTION(BlueprintPure, Category = "BG|Deck")
	int32 GetDiscardCount() const { return DiscardCount; }

	// ══════════════════════════════════════════════════════════════════
	//  턴 진행 (서버 권위)
	//
	//  TurnStart → Draw(DrawCount장) → Play → [Discard] → 다음 살아있는 좌석
	//  Draw 는 자동으로 넘어가고, Play 는 플레이어가 EndTurn 을 부를 때까지 머문다.
	//  손패가 한도(=현재 Health)를 넘으면 Discard 로 가서 버릴 때까지 대기한다.
	// ══════════════════════════════════════════════════════════════════

	/** 판 시작 — 초기 손패를 나눠주고 첫 턴을 연다. 역할 배정이 끝난 뒤 호출할 것. */
	void StartMatch();

	/** 현재 턴 좌석. 아직 시작 전이면 INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	int32 GetCurrentSeat() const { return CurrentSeat; }

	//	GetPhaseTag() 는 위 BG|Phase 섹션에 있다 (머지 중복 제거).

	/** 이 좌석이 지금 카드를 낼 수 있는가 (자기 턴 + Play 페이즈). */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	bool CanSeatPlayCards(int32 Seat) const;

	/** 이 좌석이 지금 손패를 버려야 하는가 (자기 턴 + Discard 페이즈). */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	bool CanSeatDiscard(int32 Seat) const;

	/** 턴 종료 요청. 자기 턴이 아니면 무시된다. 손패 초과면 Discard 페이즈로 간다. */
	void RequestEndTurn(int32 RequestingSeat);

	/** 손패를 한 장 버렸다는 통지. 한도 이하가 되면 턴을 넘긴다. */
	void NotifyCardDiscarded(int32 Seat);

	/** 손패 한도 = 현재 생명력(최대치가 아님). 좌석이 없으면 0. */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	int32 GetHandLimitForSeat(int32 Seat) const;

	// ── 턴당 카드 사용 한도 (GDD §5.2 / §7.1) ──

	/** 어트리뷰트 CardUseLimit. ASC 를 못 읽으면 DefaultCardUseLimit 로 폴백한다. */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	int32 GetCardUseLimitForSeat(int32 Seat) const;

	/** 이번 턴에 아직 카드를 낼 수 있는가(사용 횟수만 본다 — 턴/페이즈는 CanSeatPlayCards). */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	bool HasCardUsesLeft(int32 Seat) const;

	// ── 좌석 조회 (거리 계산·턴 진행의 공통 기준) ──

	/** 살아 있는 좌석만 좌석 번호 순으로. 사망자는 원에서 빠지므로 거리도 함께 바뀐다. */
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	TArray<int32> GetAliveSeatsInTableOrder() const;

	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	class ABGPlayerState* GetPlayerStateBySeat(int32 Seat) const;

	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	bool IsSeatAlive(int32 Seat) const;

	/** 좌석의 역할 태그(Role.*). 캐릭터가 없으면 무효 태그. */
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	FGameplayTag GetSeatRole(int32 Seat) const;

	// ══════════════════════════════════════════════════════════════════
	//  사망 / 승패
	// ══════════════════════════════════════════════════════════════════

	/**
	 * 서버 전용. 좌석이 사망했다. 캐릭터가 사망을 확정한 뒤 호출한다.
	 *   손패·장비 정리 → 보상/벌칙 → 승패 판정 → (미결이면) 턴 진행
	 * KillerSeat 은 모르면 INDEX_NONE.
	 */
	void HandleSeatDeath(int32 DeadSeat, int32 KillerSeat);

	/** 승패가 결정됐으면 Phase.GameOver 로 전환하고 true. */
	bool CheckWinCondition();

	/** 승리 진영(Role.*). 아직 미정이면 무효 태그. */
	UFUNCTION(BlueprintPure, Category = "BG|Match")
	FGameplayTag GetWinningRole() const { return WinningRoleTag; }

	UFUNCTION(BlueprintPure, Category = "BG|Match")
	bool IsMatchOver() const { return PhaseTag == GameOverPhaseTag(); }

	/** 좌석의 현재 Health. ASC 를 못 찾으면 0. */
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	int32 GetSeatHealth(int32 Seat) const;

protected:
	//	헤더에서 태그 상수를 직접 못 쓰므로(네이티브 태그는 .cpp 정의) 래퍼로 감싼다.
	static FGameplayTag GameOverPhaseTag();

	// ── 턴 진행 내부 ──
	void BeginTurn(int32 Seat);
	void EnterDrawPhase();
	void EnterPlayPhase();
	void AdvanceToNextSeat();

	//	서버 시작 시 덱을 준비한다.
	virtual void BeginPlay() override;

	void ShuffleDeck();
	void ReshuffleDeck();

	//	Deck / Discard / DiscardTop 을 건드린 뒤 공개 카운트를 다시 맞춘다.
	void RefreshPublicCounts();
	
	// 서버 전용
	UPROPERTY()
	TArray<FBGCardInstance> Deck;
	UPROPERTY()
	TArray<FBGCardInstance> Discard;

	// 공개 정보 
	UPROPERTY(Replicated) 
	int32 DeckCount = 0;
	UPROPERTY(Replicated) 
	int32 DiscardCount = 0;
	
	//	버린 패 사용하는 효과 제거 예정
	UPROPERTY(Replicated) 
	FBGCardInstance DiscardTop;   // 버린 패 맨 위 1장은 공개 정보
	
	//	현재 진행상황 태그
	UPROPERTY(Replicated)
	FGameplayTag PhaseTag;          // Phase.*

	//	현재 턴 좌석. 전원 공개 — 좌석 UI 의 강조와 카드 사용 검증이 이 값을 본다.
	UPROPERTY(Replicated)
	int32 CurrentSeat = INDEX_NONE;

	//	승리 진영. 판이 끝나면 채워진다.
	UPROPERTY(Replicated)
	FGameplayTag WinningRoleTag;

	//	무법자를 사살했을 때 가해자가 뽑는 장수 (뱅 원작 3장).
	UPROPERTY(EditDefaultsOnly, Category = "BG|Rule", meta = (ClampMin = "0"))
	int32 OutlawKillReward = 3;

	//	턴 시작 시 뽑는 장수의 폴백. 정상 경로는 어트리뷰트 DrawCount 를 읽는다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Turn", meta = (ClampMin = "1"))
	int32 DefaultDrawCount = 2;

	//	CardUseLimit 을 읽지 못했을 때의 폴백. GDD §7.1 초기값과 맞춘다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Turn", meta = (ClampMin = "1"))
	int32 DefaultCardUseLimit = 2;

	//	판 시작 시 나눠줄 초기 손패. 0 이하면 좌석의 Health 만큼 준다(뱅 원작 규칙).
	UPROPERTY(EditDefaultsOnly, Category = "BG|Turn")
	int32 InitialHandSize = 0;

	//	판이 진행 중인가. 무한 루프 방지 겸 턴 검증의 첫 조건이다.
	//	⚠️ 반드시 복제할 것 — CanSeatPlayCards / CanSeatDiscard 를 클라도 호출한다
	//	   (손패의 bPlayable 계산과 플레이 존 드롭 판정이 여기 달려 있다).
	//	   복제하지 않으면 클라에서 항상 false 라 자기 차례에도 카드가 나가지 않는다.
	UPROPERTY(Replicated)
	bool bMatchRunning = false;
	
	//	⚠️ 덱 셔플 전용 스트림이다. 주사위 판정은 UBGDiceComponent 가 별도 스트림을 쓴다
	//	   (한 스트림을 공유하면 판정 횟수가 셔플 결과를 밀어 재현이 깨진다).
	UPROPERTY()
	FRandomStream RandomStream;
	//	시드 보존을 위해 한번 초기화 하면 다시는 초기화 하지 않는다.
	//	게임 초기화시에만 false로 되돌릴 것.
	UPROPERTY()
	bool bInitialized = false;

	//	확률 판정 전담 컴포넌트. 매치 시드로 InitializeDeck 에서 함께 초기화된다.
	UPROPERTY(VisibleAnywhere, Category = "BG|Dice")
	TObjectPtr<UBGDiceComponent> Dice;
};

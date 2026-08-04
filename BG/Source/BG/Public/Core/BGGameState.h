// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Card/BGCardType.h"
#include "Card/BGGameDataTypes.h"   // FBGCardResolution — 멀티캐스트 인자라 완전한 타입이 필요하다
#include "GameplayTagContainer.h"
#include "GameFramework/GameStateBase.h"
#include "BGGameState.generated.h"


/**
 *
 */
class UBGDiceComponent;

/**
 * 판이 끝난 뒤의 연출 단계.
 *
 * ⚠️ 이 값은 서버가 소유하고 복제된다 — 클라가 자기 타이머로 넘어가면 안 된다.
 *    "이벤트" 가 아니라 "상태" 로 둔 이유: 늦게 접속한 클라도 지금 어느 단계인지
 *    그대로 받는다. 이벤트였다면 놓친 사람은 영영 못 받는다.
 */
UENUM(BlueprintType)
enum class EBGMatchEndStage : uint8
{
	None    UMETA(DisplayName = "진행 중"),
	Victory UMETA(DisplayName = "승리 연출"),
	Credits UMETA(DisplayName = "엔딩 크레딧")
};

/** 승패가 확정됐다. WinningRole 과 자기 역할을 비교해 승/패를 표시하면 된다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGMatchWon, FGameplayTag, WinningRole);

/** 승리 연출이 끝나고 엔딩 크레딧으로 넘어갔다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGEndingCredits);
/** 판정 결과가 도착했다. 서버·클라 양쪽에서 불린다 (GDD §4.1 — 전원이 같은 결과를 본다). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBGCardResolved, const FBGCardResolution& /*Resolution*/);

UCLASS()
class BG_API ABGGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABGGameState();

	/**
	 * 판정 결과를 전원에게 알린다 (서버에서만 호출).
	 *
	 * PlayerController 의 멀티캐스트는 소유 클라에만 가므로 여기가 아니면 남의 판정을 볼 수 없다.
	 * GameState 는 모두에게 복제되므로 좌석 전원이 같은 결과를 받는다.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CardResolved(const FBGCardResolution& Resolution);

	/** 위 멀티캐스트가 도착하면 발사된다. UI(PlayerController)가 구독한다. */
	FOnBGCardResolved OnCardResolved;

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

	// ── 종료 연출 (승리 → 크레딧 → 로비 복귀) ──────────────────────
	//
	//  서버가 단계를 넘기고, 클라는 복제된 단계를 받아 델리게이트만 쏜다.
	//  BP 바인딩 예) Get Game State → Cast To BGGameState → Bind Event to OnMatchWon
	//
	//  ⚠️ 위젯이 승리보다 늦게 만들어지면 이벤트를 놓친다. 바인딩 직후
	//     GetMatchEndStage() 를 한 번 확인해 이미 진행 중이면 즉시 처리할 것.

	UPROPERTY(BlueprintAssignable, Category = "BG|Match")
	FOnBGMatchWon OnMatchWon;

	UPROPERTY(BlueprintAssignable, Category = "BG|Match")
	FOnBGEndingCredits OnEndingCredits;

	UFUNCTION(BlueprintPure, Category = "BG|Match")
	EBGMatchEndStage GetMatchEndStage() const { return MatchEndStage; }

	/**
	 * 서버 전용. 승리 연출을 앞당겨 끝내고 크레딧으로 넘어간다.
	 * 연출 위젯의 애니메이션이 끝나는 시점에 부르면 VictorySequenceSeconds 를 기다리지 않는다.
	 * Victory 단계가 아니면 아무것도 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Match")
	void StartEndingCredits();

	/** 좌석의 현재 Health. ASC 를 못 찾으면 0. */
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	int32 GetSeatHealth(int32 Seat) const;

protected:
	//	헤더에서 태그 상수를 직접 못 쓰므로(네이티브 태그는 .cpp 정의) 래퍼로 감싼다.
	static FGameplayTag GameOverPhaseTag();

	// ── 종료 연출 내부 ──
	//
	//	서버 전용. 단계 전환의 유일한 입구다 (SetPhaseTag 와 같은 원칙).
	//	값을 바꾸고, 서버 자신의 브로드캐스트와 다음 단계 예약까지 여기서 한다.
	void SetMatchEndStage(EBGMatchEndStage NewStage);

	//	서버·클라 공통. 현재 단계에 맞는 델리게이트를 쏜다.
	//	⚠️ 리슨서버 호스트는 자기 GameState 의 OnRep 을 받지 못한다 —
	//	   그래서 서버는 SetMatchEndStage 에서, 클라는 OnRep 에서 이 함수로 모인다.
	void HandleMatchEndStage();

	UFUNCTION()
	void OnRep_MatchEndStage();

	//	서버 전용. 같은 맵을 다시 열어 판을 통째로 되돌린다(= 로비 복귀).
	//	제자리에서 상태를 하나씩 지우지 않는 이유는 BGGameState.cpp 의 구현 주석 참고.
	void ReturnToLobby();

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

	//	종료 연출 단계. 이 값이 바뀌는 것이 곧 "승리했다 / 크레딧이다" 라는 통지다.
	UPROPERTY(ReplicatedUsing = OnRep_MatchEndStage)
	EBGMatchEndStage MatchEndStage = EBGMatchEndStage::None;

	//	이미 쏜 단계. 로컬 상태라 복제하지 않는다 — 복제 변수는 재전송될 수 있어
	//	가드가 없으면 같은 델리게이트가 두 번 나간다.
	EBGMatchEndStage BroadcastedStage = EBGMatchEndStage::None;

	FTimerHandle MatchEndStageTimer;

	//	승리 연출을 보여주는 시간. 이 뒤 크레딧으로 넘어간다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Match", meta = (ClampMin = "0"))
	float VictorySequenceSeconds = 5.f;

	//	크레딧을 보여주는 시간. 이 뒤 로비로 복귀한다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Match", meta = (ClampMin = "0"))
	float EndingCreditsSeconds = 10.f;

	//	끄면 크레딧 화면에 머문다(연출 확인용). 켜면 자동으로 로비로 돌아간다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Match")
	bool bReturnToLobbyAfterCredits = true;

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

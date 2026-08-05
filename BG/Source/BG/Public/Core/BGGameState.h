// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Card/BGCardType.h"
#include "Card/BGGameDataTypes.h"   // FBGCardResolution — 멀티캐스트 인자라 완전한 타입이 필요하다
#include "Core/BGCinematicCamera.h" // FBGCameraShot — 복제 프로퍼티라 완전한 타입이 필요하다
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

/**
 * 카드 사용 연출의 단계 (아트 기획서 "전투 연출").
 *
 *   Primary   — 사용자 캐릭터의 공격 연출
 *   Secondary — 판정 결과에 따른 피해자의 결과 연출
 *
 * ⚠️ Phase.* 와 **별개의 축**이다. PhaseTag 를 덮어쓰지 않는 이유:
 *    연출이 끝나면 원래 페이즈로 "돌아와야" 하는데, 덮어쓰면 무엇으로 돌아갈지를
 *    따로 저장해야 한다. 병렬 상태로 두면 연출 중에도 PhaseTag 가 그대로 남아
 *    복구가 곧 "연출 단계를 None 으로 되돌리는 것" 하나로 끝난다.
 */
UENUM(BlueprintType)
enum class EBGCinematicStage : uint8
{
	None      UMETA(DisplayName = "연출 아님"),
	Primary   UMETA(DisplayName = "사용자 연출"),
	Secondary UMETA(DisplayName = "피해자 연출")
};

/**
 * 연출이 끝나기를 기다리는 사망 1건 (서버 전용).
 *
 * 좌석 번호만 담는다 — 포인터를 들면 연출이 도는 동안 대상이 파괴됐을 때 무효 참조가 된다.
 * 좌석 번호는 그런 위험이 없고, 처리 시점에 PlayerState 를 다시 찾으면 그만이다.
 */
USTRUCT()
struct FBGPendingDeath
{
	GENERATED_BODY()

	UPROPERTY()
	int32 DeadSeat = INDEX_NONE;

	UPROPERTY()
	int32 KillerSeat = INDEX_NONE;
};

/** 연출 단계가 바뀌었다. 서버·클라 양쪽에서 불린다. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBGCinematicStageChanged,
	EBGCinematicStage /*Stage*/, class ABGCharacter* /*Subject*/);

/** 승패가 확정됐다. WinningRole 과 자기 역할을 비교해 승/패를 표시하면 된다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGMatchWon, FGameplayTag, WinningRole);

/** 승리 연출이 끝나고 엔딩 크레딧으로 넘어갔다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGEndingCredits);
/** 판정 결과가 도착했다. 서버·클라 양쪽에서 불린다 (GDD §4.1 — 전원이 같은 결과를 본다). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBGCardResolved, const FBGCardResolution& /*Resolution*/);

/** 연출 카메라 샷이 바뀌었다. 서버·클라 양쪽에서 불린다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBGCameraShotChanged, const FBGCameraShot& /*Shot*/);

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

	/**
	 * 손패 상한. 좌석과 무관하게 MaxHandSize 다.
	 *
	 * ⚠️ [비활성] 예전에는 뱅 원작대로 **현재 생명력**이 한도였고, 턴 종료 시 초과분을
	 *    버려야 턴이 넘어갔다(Phase.Discard). 지금은 상한을 드로우 시점에 막으므로
	 *    체력과 무관하다 — 체력이 줄어도 이미 든 카드를 버리지 않는다.
	 *    되살리려면 이 함수가 GetSeatHealth(Seat) 를 돌려주게 하면 된다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	int32 GetHandLimitForSeat(int32 Seat) const;

	/**
	 * 서버 전용. 덱에서 최대 Count 장을 뽑아 손패에 넣는다. **실제로 뽑은 장수**를 반환한다.
	 *
	 * 덱에서 카드를 빼기 **전에** 손패 상한을 본다 — 순서가 반대면 뽑아 놓고 넣지 못한
	 * 카드가 덱에서 사라진다.
	 *
	 * ⚠️ 상한에 걸려 0 장을 뽑아도 그것은 **실패가 아니다.** 드로우 효과를 가진 카드는
	 *    정상적으로 사용된 것으로 처리해야 한다(같은 카드의 다른 효과는 그대로 일어난다).
	 *    덱에서 뽑는 모든 경로는 이 함수를 쓸 것 — 직접 DrawFromDeck 을 부르면 상한을 우회한다.
	 */
	int32 DrawCardsToHand(class ABGPlayerState* PS, int32 Count);

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
	 *
	 * ⚠️ **연출 중이면 처리를 미룬다.** 규칙 처리(손패 정리·보상/벌칙·승패·턴 진행)가
	 *    연출 도중에 끼어들면 화면에서는 아직 살아 있는 캐릭터의 카드가 버려지고,
	 *    승패 판정이 먼저 나 버린다. 연출이 끝난 뒤 순서대로 처리한다.
	 *
	 * KillerSeat 은 모르면 INDEX_NONE.
	 */
	void HandleSeatDeath(int32 DeadSeat, int32 KillerSeat);

	/** 연출이 끝나기를 기다리는 사망이 있는가. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	bool HasPendingDeaths() const { return PendingDeaths.Num() > 0; }

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

	// ── 연출 카메라 (GDD §10 "짧은 연출") ─────────────────────────
	//
	//  복제되는 것은 "지금 누구의 B 카메라를 비추는가" 라는 상태 하나뿐이다.
	//  카메라의 실제 위치는 각 클라가 로컬에서 보간한다(ABGCinematicCamera).
	//
	//  ⚠️ MatchEndStage 와 같은 이유로 이벤트가 아니라 상태다 — 늦게 온 클라도
	//     지금 어느 샷인지 그대로 받아 따라잡는다.

	/**
	 * 서버 전용. 특정 캐릭터의 B 카메라로 연출을 전환한다.
	 * Target 이 null 이면 연출을 끝내고 전원이 자기 A 카메라로 돌아간다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Camera")
	void SetCameraShot(class ABGCharacter* Target, float BlendSeconds = 1.5f,
		EBGShotBlend Blend = EBGShotBlend::Orbit);

	UFUNCTION(BlueprintPure, Category = "BG|Camera")
	const FBGCameraShot& GetCameraShot() const { return CameraShot; }

	/** 샷이 바뀌었다. PlayerController 가 구독해 뷰타깃과 카메라 액터를 갱신한다. */
	FOnBGCameraShotChanged OnCameraShotChanged;

	// ── 카드 연출 (아트 기획서 "전투 연출") ───────────────────────
	//
	//  Card.Trait.Cinematic 이 붙은 카드를 쓰면 전원이 연출 상태가 되고,
	//  Primary(사용자) → Secondary(피해자) 를 거쳐 원래 상태로 돌아온다.
	//
	//  ⚠️ 연출은 **규칙에 관여하지 않는다.** 판정·효과 적용·카드 소비는 이 함수가 불리기
	//     전에 이미 서버에서 끝나 있다. 여기서 하는 일은 그동안 입력을 막고 단계를 알리는 것뿐이다.

	/**
	 * 서버 전용. 연출을 시작한다. 이미 연출 중이면 무시한다(카드 두 장이 겹쳐 들어오는 경우).
	 *
	 * Caster : 카드를 쓴 캐릭터 — Primary 단계의 주인공.
	 * Victim : 피해자 — Secondary 단계의 주인공. 없으면(자기 대상 카드 등) Caster 를 다시 쓴다.
	 *
	 * ⚠️ 인자 이름을 Instigator 로 쓸 수 없다 — AActor::Instigator 와 겹쳐 UHT 가 거부한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Cinematic")
	void BeginCardCinematic(class ABGCharacter* Caster, class ABGCharacter* Victim);

	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	EBGCinematicStage GetCinematicStage() const { return CinematicStage; }

	/** 지금 연출 중인가. 연출 중에는 아무도 카드를 낼 수 없다. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	bool IsCinematicPlaying() const { return CinematicStage != EBGCinematicStage::None; }

	/** 현재 단계의 주인공. 연출 중이 아니면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	class ABGCharacter* GetCinematicSubject() const;

	/** 단계 전환 통지. 위젯·사운드 등이 구독한다. */
	FOnBGCinematicStageChanged OnCinematicStageChanged;

	// ── 연출 카메라 속도 ──────────────────────────────────────────
	//
	//  세 구간의 성격이 다르다. 한곳에 모아 둔 이유는 이 셋이 하나의 연속된 체감으로
	//  묶이기 때문이다 — 따로 두면 "진입만 느린" 조합을 만들어 놓고 원인을 못 찾는다.
	//
	//    진입 : 평상시 시점 → 연출 카메라   (뷰타깃 전환)
	//    단계 : 사용자 샷  → 피해자 샷      (카메라 액터가 직접 이동)
	//    종료 : 연출 카메라 → 자기 시점     (뷰타깃 전환)

	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	float GetCinematicEnterBlendSeconds() const { return CinematicEnterBlendSeconds; }

	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	float GetCinematicExitBlendSeconds() const { return CinematicExitBlendSeconds; }

	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	float GetCinematicShotBlendSeconds() const { return CinematicShotBlendSeconds; }

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
	// ── 사망 처리 내부 ──
	//
	//	실제 규칙 처리. HandleSeatDeath 가 연출 여부를 보고 지금 부를지 미룰지 정한다.
	//	직접 부르지 말 것 — 연출 중 호출을 막는 관문을 건너뛰게 된다.
	void ProcessSeatDeath(int32 DeadSeat, int32 KillerSeat);

	/**
	 * 미뤄 둔 사망을 발생 순서대로 처리한다. 연출이 끝날 때 불린다.
	 *
	 * 🔜 사망 연출은 여기에 들어온다 — 지금은 규칙 처리로 바로 넘어가지만,
	 *    "사망자 확인 → 사망 연출(여럿이면 순차) → 규칙 처리" 순서가 되면
	 *    이 함수가 연출을 하나씩 재생한 뒤 ProcessSeatDeath 를 부르는 형태가 된다.
	 */
	void FlushPendingDeaths();

	//	서버 전용. 복제하지 않는다 — 클라가 알 필요가 없고, 알면 사망을 미리 보게 된다.
	UPROPERTY()
	TArray<FBGPendingDeath> PendingDeaths;

	// ── 카드 연출 내부 ──
	//
	//	서버 전용. 단계 전환의 유일한 입구다 (SetPhaseTag / SetMatchEndStage 와 같은 원칙).
	void SetCinematicStage(EBGCinematicStage NewStage);

	//	서버·클라 공통. 현재 단계에 맞춰 델리게이트를 쏘고 카메라를 맞춘다.
	void HandleCinematicStage();

	UFUNCTION()
	void OnRep_CinematicStage();

	UPROPERTY(ReplicatedUsing = OnRep_CinematicStage)
	EBGCinematicStage CinematicStage = EBGCinematicStage::None;

	//	연출의 두 주인공. 단계와 같은 번들로 복제되므로 OnRep 시점에 이미 채워져 있다.
	UPROPERTY(Replicated)
	TObjectPtr<class ABGCharacter> CinematicInstigator;

	UPROPERTY(Replicated)
	TObjectPtr<class ABGCharacter> CinematicTarget;

	//	복제 재전송으로 같은 델리게이트가 두 번 나가는 것을 막는다(BroadcastedStage 와 같은 이유).
	EBGCinematicStage BroadcastedCinematicStage = EBGCinematicStage::None;

	//	타이머는 서버에만 하나 돈다. 클라가 각자 세면 늦게 들어온 사람이 어긋난다.
	FTimerHandle CinematicStageTimer;

	//	사용자 공격 연출 길이(초). 0 이하면 이 단계를 건너뛴다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Cinematic", meta = (ClampMin = "0.0"))
	float PrimaryCinematicSeconds = 3.f;

	//	피해자 결과 연출 길이(초). 0 이하면 이 단계를 건너뛴다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Cinematic", meta = (ClampMin = "0.0"))
	float SecondaryCinematicSeconds = 3.f;

	/**
	 * 연출 진입 시 뷰타깃 블렌드(초). **0 이면 즉시 전환**한다.
	 *
	 * 0 을 기본으로 둔 이유: 연출의 첫 프레임은 "장면이 바뀌었다" 가 분명해야 한다.
	 * 여기서 보간하면 자기 시점에서 연출 카메라로 미끄러지듯 넘어가 전환이 흐려진다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Cinematic", meta = (ClampMin = "0.0"))
	float CinematicEnterBlendSeconds = 0.f;

	/** 연출 종료 시 뷰타깃 블렌드(초). 0 이면 즉시 자기 시점으로 돌아온다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Cinematic", meta = (ClampMin = "0.0"))
	float CinematicExitBlendSeconds = 0.f;

	/**
	 * 단계 사이(사용자 샷 → 피해자 샷) 카메라 이동 시간(초). 0 이하면 순간이동.
	 *
	 * ⚠️ 단계 길이(Primary/SecondaryCinematicSeconds)보다 짧게 둘 것.
	 *    이동이 단계보다 길면 도착하기도 전에 다음 단계로 넘어가 샷을 제대로 못 본다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Cinematic", meta = (ClampMin = "0.0"))
	float CinematicShotBlendSeconds = 0.6f;

	//	연출 카메라 상태. 트랜스폼이 아니라 "의도" 만 복제한다 — 보간은 각 클라가 로컬에서.
	UPROPERTY(ReplicatedUsing = OnRep_CameraShot)
	FBGCameraShot CameraShot;

	UFUNCTION()
	void OnRep_CameraShot();

	//	서버·클라 공통 진입점. 서버는 SetCameraShot 에서, 클라는 OnRep 에서 여기로 모인다
	//	(리슨서버 호스트는 자기 OnRep 을 받지 못한다).
	void HandleCameraShot();

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

	/**
	 * 손패 상한. 이 장수를 넘기는 드로우는 넘는 만큼만 뽑히지 않는다.
	 *
	 * 뱅 원작의 "턴 종료 시 체력만큼만 남긴다" 를 대체한 규칙이다 — 버리기를 강요하지 않고
	 * 드로우 쪽에서 막는다. 0 이하면 상한을 두지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Turn", meta = (ClampMin = "0"))
	int32 MaxHandSize = 10;

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

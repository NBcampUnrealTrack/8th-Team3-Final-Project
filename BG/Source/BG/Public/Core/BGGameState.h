// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Card/BGCardType.h"
#include "Card/BGGameDataTypes.h"   // FBGCardResolution — 멀티캐스트 인자라 완전한 타입이 필요하다
#include "Core/BGCinematicCamera.h" // FBGCameraShot — 복제 프로퍼티라 완전한 타입이 필요하다
#include "Core/BGIntroStage.h"      // EBGIntroStage — 복제 프로퍼티라 완전한 타입이 필요하다
#include "GameplayTagContainer.h"
#include "Ping/BGPingTypes.h"   // FBGPingEvent — 멀티캐스트 인자라 완전한 타입이 필요하다
#include "GameFramework/GameStateBase.h"
#include "BGGameState.generated.h"

/**
 *
 */
class UBGDiceComponent;
class UAbilitySystemComponent;

/**
 * 판이 끝난 뒤의 연출 단계.
 *
 * 주의: 이 값은 서버가 소유하고 복제된다 — 클라가 자기 타이머로 넘어가면 안 된다.
 * "이벤트" 가 아니라 "상태" 로 둔 이유: 늦게 접속한 클라도 지금 어느 단계인지
 * 그대로 받는다. 이벤트였다면 놓친 사람은 영영 못 받는다.
 */
UENUM(BlueprintType)
enum class EBGMatchEndStage : uint8
{
	None    UMETA(DisplayName = "진행 중"),
	Victory UMETA(DisplayName = "승리 연출"),
	Credits UMETA(DisplayName = "결과 화면"),

	//	⚠️ 값은 **끝에만** 추가할 것. 중간에 끼우면 기존 값의 번호가 밀려 BP 의 Switch 핀이 어긋난다.
	//	   흐름 순서는 코드가 정한다 — 번호 순서와 무관하다.
	FinalReveal UMETA(DisplayName = "마지막 정체 공개"),
	EndingSequence UMETA(DisplayName = "엔딩 시퀀스")
};
//	흐름: FinalReveal(사망자 비춤) → Victory(승리/패배 위젯) → EndingSequence(성 시퀀스) → Credits(결과 표)

/** 종료 연출 단계가 바뀌었다 — BP(HUD·엔딩 위젯)가 여기에 자기 연출을 건다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGMatchEndStageChangedBP, EBGMatchEndStage, Stage);

/**
 * 연출의 단계 (아트 기획서 "전투 연출" / "사망 연출").
 *
 * 카드 연출 — 카드 1장에 대해 한 번
 *   Primary   : 사용자 캐릭터의 공격 연출
 *   Secondary : 피해자의 피격 연출.
 *               ⚠️ 여기서는 **사망 여부를 보지 않는다.** 죽는 일격이어도 평소와 같은
 *                  피격 연출을 재생한다 — 쓰러지는 것은 아래 사망 연출의 몫이다.
 *
 * 사망 연출 — 카드 연출이 끝난 뒤, 이번에 죽은 사람 수만큼 순차로
 *   DeathPrimary   : 사망자 카메라로 전환 + 쓰러지는 연출
 *   DeathSecondary : 역할 공개 연출 (이 단계에 들어가는 순간 역할이 실제로 공개된다)
 *   → 이 단계가 끝나야 규칙이 돈다 (카드 정리·보상/벌칙·승패·턴).
 *
 * 두 연출이 같은 축을 쓰는 이유: 입력 차단(CanSeatPlayCards)·밧줄 정지·카메라 전환이
 * 전부 IsCinematicPlaying() 하나에 걸려 있다. 축을 나누면 그 셋을 각각 두 번 배선해야 한다.
 *
 * 주의: Phase.* 와는 **별개의 축**이다. PhaseTag 를 덮어쓰지 않는 이유:
 * 연출이 끝나면 원래 페이즈로 "돌아와야" 하는데, 덮어쓰면 무엇으로 돌아갈지를
 * 따로 저장해야 한다. 병렬 상태로 두면 연출 중에도 PhaseTag 가 그대로 남아
 * 복구가 곧 "연출 단계를 None 으로 되돌리는 것" 하나로 끝난다.
 *
 * ⚠️ 값은 끝에 추가할 것 — BP·ABP 가 이미 이 열거형을 참조한다.
 */
UENUM(BlueprintType)
enum class EBGCinematicStage : uint8
{
	None           UMETA(DisplayName = "연출 아님"),
	Primary        UMETA(DisplayName = "사용자 연출"),
	Secondary      UMETA(DisplayName = "피해자 연출"),
	DeathPrimary   UMETA(DisplayName = "사망 연출"),
	DeathSecondary UMETA(DisplayName = "역할 공개 연출")
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

/** 페이즈·현재 좌석·판 진행 여부 중 하나라도 바뀌었다. 턴 UI 가 구독한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGTurnStateChanged);
/** 판정 결과가 도착했다. 서버·클라 양쪽에서 불린다 (GDD §4.1 — 전원이 같은 결과를 본다). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBGCardResolved, const FBGCardResolution& /*Resolution*/);

/** 장비 착용 통지. 판정이 없어 FBGCardResolution 을 쓸 수 없다 — 좌석과 카드 종류만 오간다. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBGCardEquipped,
	int32 /*SeatIndex*/, FGameplayTag /*CardId*/);

/** 연출 카메라 샷이 바뀌었다. 서버·클라 양쪽에서 불린다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBGCameraShotChanged, const FBGCameraShot& /*Shot*/);

/** 채팅 메시지 '1'건 도착시 UI로 중계 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBGChatMessageReceived, const FString& /*SenderName*/, const FString& /*Message*/)

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
	
	/** 채팅 메시지를 전원에게 알린다 (서버에서만 호출) */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ChatMessage(const FString& SenderName, const FString& Message);
	
	FOnBGChatMessageReceived OnChatMessageReceived;

	/** 핑 1건을 전원에게 알린다 (서버 검증 통과 후에만 호출). */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Ping(const FBGPingEvent& Event);

	/** 위 멀티캐스트가 도착하면 발사된다. 표시 컴포넌트가 구독한다. */
	FOnBGPingReceived OnPingReceived;

	/**
	 * 장비를 착용했다고 전원에게 알린다 (서버에서만 호출).
	 *
	 * ⚠️ Multicast_CardResolved 를 재사용하지 않는 이유: 장비는 판정을 굴리지 않는다.
	 *    등급도 눈도 없는 결과를 그 경로로 흘리면 판정 위젯이 주사위와 등급을 지어내 띄운다.
	 *    여기로 오는 것은 "누가 무엇을 착용했다" 뿐이고, 화면에 세울 카드 연출이 그 용도다.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CardEquipped(int32 SeatIndex, FGameplayTag CardId);

	/** 위 멀티캐스트가 도착하면 발사된다. */
	FOnBGCardEquipped OnCardEquipped;

	/**
	 * 장비를 착용했다고 전원에게 알린다 (서버에서만 호출).
	 *
	 * ⚠️ Multicast_CardResolved 를 재사용하지 않는 이유: 장비는 판정을 굴리지 않는다.
	 *    등급도 눈도 없는 결과를 그 경로로 흘리면 판정 위젯이 주사위와 등급을 지어내 띄운다.
	 *    여기로 오는 것은 "누가 무엇을 착용했다" 뿐이고, 화면에 세울 카드 연출이 그 용도다.
	 */

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//	확률 판정(주사위) 전담. 어디서든 UBGDiceComponent::Get(this) 로도 얻을 수 있다.
	UFUNCTION(BlueprintPure, Category = "BG|Dice")
	UBGDiceComponent* GetDice() const { return Dice; }

	//	서버에서 덱을 구성·셔플한다. 이미 초기화됐으면 아무것도 하지 않는다(시드 보존).
	//	Seed == 0 이면 새 시드를 뽑아 쓰고, 사용한 값을 로그에 남긴다 — 재현에 필수.
	//	EncounterIndex 는 PvE 전용, 1-base(조우1부터). 지금은 보상 카드 여부만 본다.
	//	TODO: 조우별 난이도 스케일링을 여기에 넣는다.
	void InitializeDeck(int32 Seed = 0, int32 EncounterIndex = 1);

	// Side 생략 시 Player 덱. 소진 시 그 진영의 Discard 를 리셔플한다.
	bool DrawFromDeck(FBGCardInstance& OutCard, EBGDeckSide Side = EBGDeckSide::Player);

	void PushToDiscard(const FBGCardInstance& Card);

	/**
	 * [미구현] 뽑기(Draw!) 판정. 수트를 쓰지 않으므로 확률 굴림으로 대체할 자리다.
	 *
	 * 주의: 지금은 인자를 무시하고 항상 false 를 돌려준다 — 호출하면 조용히 전부 실패한다.
	 * 감옥·다이너마이트를 넣을 때 구현한다. 반드시 Dice 의 시드된 스트림을 쓸 것
	 * (셔플 스트림을 쓰면 판정이 덱 순서를 밀어 재현이 깨진다).
	 */
	bool PerformDrawCheck(int32 Seat, float SuccessChance);
	
	//	주의: 서버 전용 데이터다. 클라에서 부르면 항상 빈 배열이 나온다(덱 내용은 복제되지 않는다).
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

	// PvE 전용. 적 진영 덱의 공개 카운트, PvP 에선 항상 0(적 덱을 안 씀).
	UFUNCTION(BlueprintPure, Category = "BG|Deck")
	int32 GetEnemyDeckCount() const { return EnemyDeckCount; }

	UFUNCTION(BlueprintPure, Category = "BG|Deck")
	int32 GetEnemyDiscardCount() const { return EnemyDiscardCount; }

	// ══════════════════════════════════════════════════════════════════
	//  턴 진행 (서버 권위)
	//
	//  TurnStart → Draw(DrawCount장) → Play → [Discard] → 다음 살아있는 좌석
	//  Draw 는 자동으로 넘어가고, Play 는 플레이어가 EndTurn 을 부를 때까지 머문다.
	//  손패가 한도(=현재 Health)를 넘으면 Discard 로 가서 버릴 때까지 대기한다.
	// ══════════════════════════════════════════════════════════════════

	/**
	 * 판 시작 — 도입 연출을 연다. 역할 배정이 끝난 뒤 호출할 것.
	 * 손패 배분과 첫 턴은 연출이 끝나는 자리(BeginFirstTurn)에서 함께 이뤄진다.
	 */
	void StartMatch();

	// ── 도입 연출 ────────────────────────────────────────────────────
	//
	//  판이 열리기 전 12초짜리 도입부. 규칙은 이 사이 아무것도 하지 않는다 —
	//  손패 배분도 첫 턴도 연출이 끝난 뒤에 이뤄진다(BeginFirstTurn).

	/** 서버 전용. 도입 연출을 시작한다. 이미 돌고 있으면 무시한다. */
	void BeginIntro();

	/** 서버 전용. 남은 단계를 건너뛰고 바로 판을 연다(콘솔 BG_SkipIntro). */
	void SkipIntro();

	UFUNCTION(BlueprintPure, Category = "BG|Intro")
	EBGIntroStage GetIntroStage() const { return IntroStage; }

	/** 도입 연출 중인가. 그동안은 아무도 카드를 낼 수 없다. */
	UFUNCTION(BlueprintPure, Category = "BG|Intro")
	bool IsIntroPlaying() const { return IntroStage != EBGIntroStage::None; }

	/** 용사(보안관) 좌석. 공개 역할이 없으면 INDEX_NONE — 용사 공개 단계가 이 좌석을 비춘다. */
	UFUNCTION(BlueprintPure, Category = "BG|Intro")
	int32 GetHeroSeat() const;

	/** 첫 턴을 가져갈 좌석. 도입 연출이 "누구 차례인지" 를 미리 보여줄 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Intro")
	int32 GetFirstTurnSeat() const;

	/** 단계 전환 통지. HUD·이름표·손패가 각자 구독한다. */
	FOnBGIntroStageChanged OnIntroStageChanged;

	/** 보안관이 고를 수 있는 PvE 보상 후보. 없으면 빈 배열. */
	UFUNCTION(BlueprintPure, Category = "BG|PvE")
	const TArray<FName>& GetPendingRewardCandidates() const { return PendingRewardCandidates; }

	/**
	 * PvE 보상 선택을 제출한다. CardId 가 실제 후보에 없으면 무시한다(클라를 안 믿는다).
	 * 선택이 확정되면 보상 때문에 미뤄뒀던 StartMatch() 를 여기서 대신 부른다.
	 */
	void ResolvePvERewardSelection(FName CardId);

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

	/** 판이 돌고 있는가. 로비·종료 상태면 false. */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	bool IsMatchRunning() const { return bMatchRunning; }

	/**
	 * 턴 상태(페이즈·현재 좌석·진행 여부)가 바뀌었다. UI 는 폴링하지 말고 이걸 구독한다.
	 * 위젯이 늦게 생기면 첫 통지를 놓치므로, 구독 직후 한 번 직접 갱신할 것.
	 */
	UPROPERTY(BlueprintAssignable, Category = "BG|Turn")
	FOnBGTurnStateChanged OnTurnStateChanged;

	/** 턴 종료 요청. 자기 턴이 아니면 무시된다. 손패 초과면 Discard 페이즈로 간다. */
	void RequestEndTurn(int32 RequestingSeat);

	// 서버 전용. 밧줄이 다 타서 턴을 강제로 넘긴다. 본인 의사와 무관하므로 아무것도 묻지 않고
	// 바로 다음 좌석으로 간다.
	void ForceEndTurn(int32 Seat, const FString& Reason);

	// 서버 전용. 좌석을 즉사시킨다(복귀 포기 등). ASC 가 있으면 피해 GE, 없으면 직접 통지.
	// 둘 다 HandleSeatDeath 로 모이므로 승패·손패 정리가 평소대로 처리된다.
	void KillSeat(int32 Seat, const FString& Reason);

	/** 손패를 한 장 버렸다는 통지. 한도 이하가 되면 턴을 넘긴다. */
	void NotifyCardDiscarded(int32 Seat);

	/** 카드 사용이 확정됐다는 통지. 그 좌석의 밧줄을 전체 길이로 다시 채운다. */
	void NotifyCardPlayed(int32 Seat);

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
	 * 주의: **연출 중이면 처리를 미룬다.** 규칙 처리(손패 정리·보상/벌칙·승패·턴 진행)가
	 * 연출 도중에 끼어들면 화면에서는 아직 살아 있는 캐릭터의 카드가 버려지고,
	 * 승패 판정이 먼저 나 버린다. 연출이 끝난 뒤 순서대로 처리한다.
	 *
	 * KillerSeat 은 모르면 INDEX_NONE.
	 */
	void HandleSeatDeath(int32 DeadSeat, int32 KillerSeat);

	/** 연출이 끝나기를 기다리는 사망이 있는가. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	bool HasPendingDeaths() const { return PendingDeaths.Num() > 0; }

	/** 승패가 결정됐으면 Phase.GameOver 로 전환하고 true. */
	bool CheckWinCondition();

	/** 서버 전용 테스트용. 생존자와 무관하게 Winner 진영 승리로 판을 끝낸다(콘솔 BG_ForceWin). */
	void DebugForceWin(FGameplayTag Winner);

	/** 승리 진영(Role.*). 아직 미정이면 무효 태그. */
	UFUNCTION(BlueprintPure, Category = "BG|Match")
	FGameplayTag GetWinningRole() const { return WinningRoleTag; }

	UFUNCTION(BlueprintPure, Category = "BG|Match")
	bool IsMatchOver() const { return PhaseTag == GameOverPhaseTag(); }

	// ── 종료 연출 (정체 공개 → 승리 위젯 → 엔딩 시퀀스 → 결과 표 → 로비 복귀) ──
	//
	//  서버가 단계를 넘기고, 클라는 복제된 단계를 받아 OnMatchEndStageChanged 만 쏜다.
	//  화면(승리 위젯·시퀀스·결과 표)은 ABGPlayerController::HandleMatchEndStageChanged 가 맡는다.
	//
	//  주의: 승리보다 늦게 구독하면 통지를 놓친다. 구독 직후 GetMatchEndStage() 로 한 번 맞출 것.

	/**
	 * 단계 자체를 알려 준다. 승리·결과처럼 "무엇을 띄울지" 가 아니라 "지금 어느 구간인지" 가
	 * 필요한 쪽(HUD 숨김·시퀀스 재생)이 쓴다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "BG|Match")
	FOnBGMatchEndStageChangedBP OnMatchEndStageChanged;

	UFUNCTION(BlueprintPure, Category = "BG|Match")
	EBGMatchEndStage GetMatchEndStage() const { return MatchEndStage; }

	/** 엔딩 화면(승리 위젯·시퀀스·결과 표)이 떠 있는가. 게임 UI 와 연출 카메라가 물러나야 하는 구간이다. */
	UFUNCTION(BlueprintPure, Category = "BG|Match")
	bool IsEndingPresentationActive() const
	{
		return MatchEndStage == EBGMatchEndStage::Victory
			|| MatchEndStage == EBGMatchEndStage::EndingSequence
			|| MatchEndStage == EBGMatchEndStage::Credits;
	}

	/**
	 * 게임 UI(HUD·닉네임)를 걷어야 하는가. 시퀀스로 컷되는 순간부터다.
	 * 승리 위젯 단계에서는 판 화면을 그대로 둔 채 위젯만 얹는다.
	 */
	bool ShouldHideGameplayUIForEnding() const
	{
		return MatchEndStage == EBGMatchEndStage::EndingSequence
			|| MatchEndStage == EBGMatchEndStage::Credits;
	}

	/**
	 * 이 역할이 이번 판의 승자인가. 결과 화면이 좌석 줄마다 승/패를 찍을 때 쓴다.
	 * 보안관 진영이 이기면 부관도 함께 승리다(뱅 원작 규칙).
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Match")
	bool IsRoleWinner(FGameplayTag SeatRole) const;

	/**
	 * 서버 전용. 결과 화면의 "로비로" 버튼이 부른다 — 전원이 함께 돌아간다.
	 *
	 * ⚠️ 결과 화면 단계에서만 받는다. 판이 도는 중에 들어오면 무시한다 —
	 *    클라가 보낸 요청이라 시점을 믿을 수 없다.
	 */
	void RequestReturnToLobbyFromResult();

	/**
	 * 서버 전용. 엔딩 시퀀스를 앞당겨 끝내고 결과 화면으로 넘어간다.
	 * EndingSequence 단계가 아니면 아무것도 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Match")
	void StartEndingCredits();

	// ── 연출 카메라 (GDD §10 "짧은 연출") ─────────────────────────
	//
	//  복제되는 것은 "지금 누구의 B 카메라를 비추는가" 라는 상태 하나뿐이다.
	//  카메라의 실제 위치는 각 클라가 로컬에서 보간한다(ABGCinematicCamera).
	//
	//  주의: MatchEndStage 와 같은 이유로 이벤트가 아니라 상태다 — 늦게 온 클라도
	// 지금 어느 샷인지 그대로 받아 따라잡는다.

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
	//  주의: 연출은 **규칙에 관여하지 않는다.** 판정·효과 적용·카드 소비는 이 함수가 불리기
	// 전에 이미 서버에서 끝나 있다. 여기서 하는 일은 그동안 입력을 막고 단계를 알리는 것뿐이다.

	/**
	 * 서버 전용. 연출을 시작한다. 이미 연출 중이면 무시한다(카드 두 장이 겹쳐 들어오는 경우).
	 *
	 * Caster     : 카드를 쓴 캐릭터 — Primary 단계의 주인공.
	 * Victim     : 피해자 — Secondary 단계의 주인공. 없으면(자기 대상 카드 등) Caster 를 다시 쓴다.
	 * Resolution : 이 카드의 판정 결과. 애니메이션이 카드 종류와 성패로 분기하는 데 쓴다
	 *              (ABGCharacter::GetCinematicCardId / GetCinematicOutcome).
	 *
	 * 주의: 인자 이름을 Instigator 로 쓸 수 없다 — AActor::Instigator 와 겹쳐 UHT 가 거부한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Cinematic")
	void BeginCardCinematic(class ABGCharacter* Caster, class ABGCharacter* Victim,
		const FBGCardResolution& Resolution);

	/**
	 * 서버 전용. 카드 규칙 처리 성공 뒤의 공통 연출 입구.
	 *
	 * 주사위 판정 카드면 먼저 Multicast_CardResolved로 UI를 시작하고
	 * ResolutionPresentationSeconds 뒤에 월드 연출을 연다. BGCardWidget의 종료 신호에는
	 * 의존하지 않는다. 그 뒤 DT_BGCardFX와 Card.Trait.Cinematic을 보고 시네마틱 또는
	 * 단발 GameplayCue를 선택한다. PlayerController는 이 함수 호출만 담당한다.
	 */
	void PlayResolvedCardPresentation(class ABGCharacter* Caster, class ABGCharacter* Target,
		const FBGCardResolution& Resolution);

	/** 주사위/판정 UI가 끝나 월드 카드 연출을 시작하기 전까지의 서버 권위 대기 상태. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	bool IsResolutionPresentationPlaying() const { return bResolutionPresentationActive; }

	/** 판정 UI 에 주어진 시간. 클래스 기본값이라 클라에서도 서버와 같은 값이 나온다. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	float GetResolutionPresentationSeconds() const { return ResolutionPresentationSeconds + CardRevealSeconds; }

	/** 판정 연출 전에 카드를 중앙에 보여 주는 시간. 카드 위젯이 이만큼 기다린 뒤 연출을 터뜨린다. */
	float GetCardRevealSeconds() const { return CardRevealSeconds; }

	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	EBGCinematicStage GetCinematicStage() const { return CinematicStage; }

	/** 지금 연출 중인가. 카드 연출·사망 연출 어느 쪽이든 참이며, 그동안 아무도 카드를 낼 수 없다. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	bool IsCinematicPlaying() const { return CinematicStage != EBGCinematicStage::None; }

	/** 사망 연출 단계인가. 카드 연출과 구분해야 하는 UI(피해 표시 등)가 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	bool IsDeathCinematic() const
	{
		return CinematicStage == EBGCinematicStage::DeathPrimary
			|| CinematicStage == EBGCinematicStage::DeathSecondary;
	}

	/** 현재 단계의 주인공. 연출 중이 아니면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	class ABGCharacter* GetCinematicSubject() const;

	/** 카드를 쓴 캐릭터 (Primary 단계의 주인공). 연출 중이 아니면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	class ABGCharacter* GetCinematicInstigator() const { return CinematicInstigator; }

	/** 연출 대상 (Secondary 단계의 주인공). 대상 없는 카드면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	class ABGCharacter* GetCinematicTarget() const { return CinematicTarget; }

	/**
	 * 지금 연출 중인 카드의 판정 결과.
	 *
	 * Roll == INDEX_NONE 이면 판정을 굴리지 않은 카드다 — 그때 Outcome 은 의미가 없다.
	 * 연출이 끝나면 비워진다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Cinematic")
	const FBGCardResolution& GetCinematicResolution() const { return CinematicResolution; }

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

	//	Seconds 뒤 Next 단계로. 0 이하면 즉시 넘어간다. 타이머는 MatchEndStageTimer 하나만 쓴다.
	void AdvanceMatchEndStageAfter(EBGMatchEndStage Next, float Seconds);

	//	서버·클라 공통. 현재 단계에 맞는 델리게이트를 쏜다.
	//	주의: 리슨서버 호스트는 자기 GameState 의 OnRep 을 받지 못한다 —
	// 그래서 서버는 SetMatchEndStage 에서, 클라는 OnRep 에서 이 함수로 모인다.
	void HandleMatchEndStage();

	UFUNCTION()
	void OnRep_MatchEndStage();

	//	서버 전용. 같은 맵을 다시 열어 판을 통째로 되돌린다(= 로비 복귀).
	//	제자리에서 상태를 하나씩 지우지 않는 이유는 BGGameState.cpp 의 구현 주석 참고.
	void ReturnToLobby();

	// ── 턴 상태 통지 ──
	//
	//	세 값(PhaseTag / CurrentSeat / bMatchRunning)이 하나의 OnRep 으로 모인다.
	//	리슨서버 호스트는 자기 OnRep 을 받지 못하므로 서버는 값을 바꾼 자리에서 직접 부른다.
	UFUNCTION()
	void OnRep_TurnState();

	void BroadcastTurnState();

	/** 서버에서 발생한 시스템 사운드를 모든 플레이어 화면에 같은 순서로 재생한다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySystemAudio(FGameplayTag AudioTag);

	//	서버 전용. 현재 좌석과 판 진행 여부를 바꾸는 유일한 입구다(SetPhaseTag 와 같은 원칙).
	void SetCurrentSeat(int32 Seat);
	void SetMatchRunning(bool bRunning);

	// ── 턴 진행 내부 ──
	void BeginTurn(int32 Seat);
	/** 치유사(TurnStartHeal). 어트리뷰트가 0 이면 아무 일도 하지 않는다. 서버 전용. */
	void ApplyTurnStartHeal(int32 Seat);

public:
	/**
	 * 서버 전용. 카드를 손패에 **만들어 넣는다.** 실제로 넣은 장수를 반환한다.
	 *
	 * 덱에서 뽑는 것이 아니다 — 덱·버린 패의 총량이 변하지 않는다.
	 * 규칙이 카드를 주는 모든 경로(암살자 대성공, PvE 이벤트 등)가 이 함수 하나를 쓴다.
	 *
	 * **일회용인지는 호출자가 정하지 않는다.** DT_BGCard 의 `Card.Trait.Ephemeral` 을 보고
	 * 이 함수가 정한다. 그래서 "이 카드 N장 줘" 만 말하면 카드 종류에 따라 알아서 갈린다 —
	 * 일반 카드를 일회용으로 둔갑시키는 경로는 **일부러 두지 않았다.** 그런 경로가 있으면
	 * 손패의 같은 카드가 어떤 건 사용 한도를 쓰고 어떤 건 안 쓰는 상태가 되어,
	 * 플레이어가 화면만 보고는 구분할 수 없다.
	 *
	 * 일회용 카드는 InstanceId 를 덱과 겹치지 않게 **음수**로 따로 발급한다.
	 * 손패 상한(현재 체력)에 걸리면 그만큼만 들어간다. DT 에 없는 Row 면 경고만 남기고 0 을 반환한다.
	 */
	int32 GrantCards(class ABGPlayerState* PS, FGameplayTag CardId, int32 Count);

protected:
	/**
	 * 1회성 카드에 줄 다음 InstanceId. **음수로 내려간다.**
	 * 덱 카드가 1 부터 올라가므로 어느 쪽도 상대를 침범하지 않는다.
	 *
	 * ⚠️ **-1 에서 시작하면 안 된다. -1 은 `INDEX_NONE` 이다.**
	 *    `ServerRequestPlayCard_Validate` / `ServerRequestDiscardCard_Validate` 가
	 *    `InstanceId != INDEX_NONE` 으로 걸러내는데, UE 는 `_Validate` 가 false 면
	 *    **연결을 끊는다.** 즉 첫 1회성 카드를 내는 순간 그 플레이어가 매치에서 튕긴다.
	 *    호스트는 RPC 를 타지 않아 멀쩡하므로 혼자 PIE 로 돌리면 절대 재현되지 않는다.
	 *    `FBGCardInstance::IsValid()` 와 `UBGHandWidget::FindHeldCard` 도 -1 을 특별 취급한다.
	 */
	int32 NextEphemeralInstanceId = -2;

	void EnterDrawPhase();
	void EnterPlayPhase();
	void AdvanceToNextSeat();

	//	서버 시작 시 덱을 준비한다.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ShuffleDeck(EBGDeckSide Side = EBGDeckSide::Player);
	void ReshuffleDeck(EBGDeckSide Side = EBGDeckSide::Player);

	// 이 좌석이 어느 진영 덱을 쓰는가. PvE 봇 좌석이면 Enemy, 그 외엔 항상 Player.
	// (PvP 는 ABGAIPlayerController 가 존재하지 않아 항상 Player 로 떨어진다.)
	EBGDeckSide GetDeckSideForSeat(int32 Seat) const;

	//	Deck / Discard / DiscardTop 을 건드린 뒤 공개 카운트를 다시 맞춘다(양쪽 진영 모두).
	void RefreshPublicCounts();

	// 서버 전용. Player 진영 덱, PvP 에선 이것이 유일한 덱이다.
	UPROPERTY()
	TArray<FBGCardInstance> Deck;
	UPROPERTY()
	TArray<FBGCardInstance> Discard;

	// 서버 전용. Enemy 진영 덱, PvE 전용이라 PvP 에선 항상 비어 있다.
	UPROPERTY()
	TArray<FBGCardInstance> EnemyDeck;
	UPROPERTY()
	TArray<FBGCardInstance> EnemyDiscard;

	// 서버 전용. PvE 조우 승리 보상 카드 — 보안관(사람)에게만 지급한다. InstanceId == INDEX_NONE 이면 없음.
	UPROPERTY()
	FBGCardInstance PendingPvEReward;

	// 보안관이 고를 수 있는 보상 후보. 선택이 끝나면(또는 애초에 후보가 없으면) 비어 있다.
	// 클라(보상 선택 위젯)가 읽어야 하므로 복제한다.
	UPROPERTY(Replicated)
	TArray<FName> PendingRewardCandidates;

	// 공개 정보
	UPROPERTY(Replicated)
	int32 DeckCount = 0;
	UPROPERTY(Replicated)
	int32 DiscardCount = 0;

	//	버린 패 사용하는 효과 제거 예정
	UPROPERTY(Replicated)
	FBGCardInstance DiscardTop;   // 버린 패 맨 위 1장은 공개 정보

	UPROPERTY(Replicated)
	int32 EnemyDeckCount = 0;
	UPROPERTY(Replicated)
	int32 EnemyDiscardCount = 0;
	UPROPERTY(Replicated)
	FBGCardInstance EnemyDiscardTop;
	
	//	현재 진행상황 태그
	UPROPERTY(ReplicatedUsing = OnRep_TurnState)
	FGameplayTag PhaseTag;          // Phase.*

	//	현재 턴 좌석. 전원 공개 — 좌석 UI 의 강조와 카드 사용 검증이 이 값을 본다.
	UPROPERTY(ReplicatedUsing = OnRep_TurnState)
	int32 CurrentSeat = INDEX_NONE;

	//	승리 진영. 판이 끝나면 채워진다.
	UPROPERTY(Replicated)
	FGameplayTag WinningRoleTag;

	//	종료 연출 단계. 이 값이 바뀌는 것이 곧 "승리했다 / 크레딧이다" 라는 통지다.
	// ── 사망 처리 내부 ──
	//
	/**
	 * "역할 공개에 따른 결과" — 카드 정리·보상/벌칙·승패·턴 넘김.
	 *
	 * 사망 연출의 **마지막**에 불린다. 직접 부르지 말 것 —
	 * HandleSeatDeath 를 거치지 않으면 연출을 건너뛰고 결과부터 나온다.
	 */
	void ProcessSeatDeath(int32 DeadSeat, int32 KillerSeat);

	//	승자를 확정하고 종료 연출을 연다. 판정은 호출자 몫이다(CheckWinCondition / DebugForceWin).
	void EndMatch(FGameplayTag Winner);

	// ── PvE 런 승패 처리 (CheckWinCondition 전용, 다른 곳에서 부르지 말 것) ──

	//	PvE 에서 사람(Sheriff) 이 이겼을 때. 다음 조우로 진행한다.
	void HandlePvERunWin();

	//	PvE 에서 사람(Sheriff) 이 졌을 때. 런을 끝낸다.
	void HandlePvERunLoss();

	/**
	 * 죽은 좌석을 **화면상으로도** 죽은 것으로 만든다. DeathPrimary 진입 직전에 불린다.
	 *
	 * 규칙상 사망(PlayerState::bIsDead)은 체력이 0 이 되는 즉시지만, 그때 쓰러뜨리면
	 * 그 카드의 공격·피격 연출 전에 먼저 넘어간다. 두 시점을 가르는 것이 이 함수다.
	 */
	void RevealDeath(int32 DeadSeat);

	/**
	 * 죽은 좌석의 역할을 공개한다. DeathSecondary 진입 **직전**에 불린다 —
	 * 역할 공개 연출이 무엇을 공개할지 알고 시작해야 하기 때문이다.
	 */
	void RevealDeadRole(int32 DeadSeat);

	/**
	 * 대기열에서 사망 1건을 꺼내 사망 연출을 시작한다. 시작했으면 true.
	 *
	 * 연출을 걸 수 없는 사망(폰이 없는 이탈자, 길이를 0 으로 꺼 둔 경우)은
	 * 그 자리에서 규칙만 처리하고 다음 건으로 넘어간다 — 그래서 반복문이다.
	 */
	bool TryBeginNextDeathCinematic();

	/** 사망 연출 1건이 끝났다 — 규칙을 처리하고 다음 사망자로 넘어간다. */
	void FinishDeathCinematic();

	/**
	 * 카드 연출이 끝났다. 밀린 사망이 있으면 **None 을 거치지 않고** 사망 연출로 곧장 넘어간다.
	 *
	 * ⚠️ 사이에 None 이 한 프레임이라도 끼면 카메라가 각자 시점으로 튀었다가 다시 날아간다.
	 *    (PlayerController 가 None 을 "연출 종료" 로 보고 뷰타깃을 자기 폰으로 되돌린다)
	 */
	void EndCardCinematic();

	//	다음 단계를 예약한다. DelaySeconds 가 0 이하면 기다리지 않고 즉시 실행한다 —
	//	연출을 끄고 규칙만 돌리고 싶을 때 길이를 0 으로 두면 된다.
	void ScheduleCinematicStep(float DelaySeconds, TFunction<void()>&& Step);

	//	서버 전용. 복제하지 않는다 — 클라가 알 필요가 없고, 알면 사망을 미리 보게 된다.
	UPROPERTY()
	TArray<FBGPendingDeath> PendingDeaths;

	//	지금 사망 연출을 재생 중인 1건. DeadSeat 이 INDEX_NONE 이면 재생 중이 아니다.
	//	대기열에서 꺼내 여기로 옮기는 이유: 연출이 도는 동안 새 사망이 대기열에 들어와도
	//	지금 재생 중인 건과 섞이지 않는다.
	UPROPERTY()
	FBGPendingDeath ActiveDeath;

	// ── 카드 연출 내부 ──
	//	서버 전용. 단계 전환의 유일한 입구다 (SetPhaseTag / SetMatchEndStage 와 같은 원칙).
	void SetCinematicStage(EBGCinematicStage NewStage);

	/** 현재 단계의 지속형 GameplayCue를 대상 캐릭터 ASC에 추가한다. 서버 전용. */
	void AddCardGameplayCueForStage(EBGCinematicStage Stage);

	/** 이전 단계 GameplayCue를 제거해 OnRemove 종료 연출을 시작한다. 서버 전용. */
	void RemoveActiveCardGameplayCue();

	/** 비시네마틱 카드의 단발 Cue를 시작/종료한다. 서버 전용. */
	void AddNonCinematicCardGameplayCue(class ABGCharacter* Caster, class ABGCharacter* Target,
		const FBGCardResolution& Resolution, float ActiveSeconds);
	void RemoveActiveNonCinematicCardGameplayCue();

	TWeakObjectPtr<UAbilitySystemComponent> ActiveCardCueASC;
	FGameplayTag ActiveCardCueTag;
	TWeakObjectPtr<UAbilitySystemComponent> ActiveNonCinematicCardCueASC;
	FGameplayTag ActiveNonCinematicCardCueTag;
	FTimerHandle NonCinematicCardCueTimer;

	/** 판정 UI 대기 없이 기존 시네마틱/단발 Cue 분기를 실행한다. 서버 전용. */
	void PlayResolvedCardPresentationNow(class ABGCharacter* Caster, class ABGCharacter* Target,
		const FBGCardResolution& Resolution);

	/** 서버 판정 연출 타이머가 끝난 뒤 보관한 월드 연출을 시작한다. */
	void FinishResolutionPresentation();

	UPROPERTY(ReplicatedUsing = OnRep_TurnState)
	bool bResolutionPresentationActive = false;

	UPROPERTY()
	TObjectPtr<class ABGCharacter> PendingPresentationCaster;

	UPROPERTY()
	TObjectPtr<class ABGCharacter> PendingPresentationTarget;

	UPROPERTY()
	FBGCardResolution PendingPresentationResolution;

	FTimerHandle ResolutionPresentationTimer;

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

	//	연출 중인 카드의 판정 결과. 위 둘과 같은 번들로 복제되므로 OnRep 시점에 이미 채워져 있다.
	//	애니메이션이 "무슨 카드를 / 성공했는지" 로 분기하려면 이 정보가 클라에도 있어야 한다 —
	//	Multicast_CardResolved 는 그 순간 한 번뿐이라 늦게 들어온 클라는 받지 못한다.
	UPROPERTY(Replicated)
	FBGCardResolution CinematicResolution;

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

	//	쓰러지는 연출 길이(초). 0 이하면 이 단계를 건너뛴다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Cinematic", meta = (ClampMin = "0.0"))
	float DeathPrimarySeconds = 3.f;

	//	역할 공개 연출 길이(초). 0 이하면 이 단계를 건너뛴다.
	//	⚠️ 둘 다 0 이면 사망 연출이 통째로 꺼지고 규칙만 즉시 돈다(이주 전 동작).
	UPROPERTY(EditDefaultsOnly, Category = "BG|Cinematic", meta = (ClampMin = "0.0"))
	float DeathSecondarySeconds = 3.f;

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
	 * 주의: 단계 길이(Primary/SecondaryCinematicSeconds)보다 짧게 둘 것.
	 * 이동이 단계보다 길면 도착하기도 전에 다음 단계로 넘어가 샷을 제대로 못 본다.
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

	//	마지막으로 정체가 공개된 좌석(= 판을 끝낸 사망자). 종료 연출 첫 단계가 이 좌석을 비춘다.
	int32 LastRevealedSeat = INDEX_NONE;

	//	마지막 사망자의 정체를 보여주는 시간. 이 뒤 승리 연출로 넘어간다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Match", meta = (ClampMin = "0"))
	float FinalRevealSeconds = 1.5f;

	//	승리/패배 위젯을 보여주는 시간. 이 뒤 엔딩 시퀀스로 넘어간다.
	//	⚠️ 위젯 등장 애니메이션보다 짧으면 애니메이션이 끝나기 전에 시퀀스로 잘린다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Match", meta = (ClampMin = "0"))
	float VictoryWidgetSeconds = 3.f;

	//	시퀀스만 보여 주는 시간. 이 뒤 결과 표가 뜨고, 시퀀스는 그 뒤에서 계속 재생된다.
	//	⚠️ 시퀀스 전체 길이(75초)를 쓰면 안 된다 — 뒷부분은 결과 표 배경으로 깔리도록 만든 구간이다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Match", meta = (ClampMin = "0"))
	float EndingSequenceSeconds = 6.f;

	//	크레딧을 보여주는 시간. 이 뒤 로비로 복귀한다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Match", meta = (ClampMin = "0"))
	float EndingCreditsSeconds = 60.f;

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

	// ── 도입 연출 내부 ──
	//	단계 전환의 유일한 입구. 다음 단계 예약까지 여기서 한다(SetCinematicStage 와 같은 원칙).
	void SetIntroStage(EBGIntroStage NewStage);

	//	서버·클라 공통 진입점. 서버는 SetIntroStage 에서, 클라는 OnRep 에서 여기로 모인다.
	void HandleIntroStage();

	UFUNCTION()
	void OnRep_IntroStage();

	//	이번 단계가 끝났다. 다음 단계로 넘기거나, 마지막이면 판을 연다.
	void AdvanceIntroStage();

	//	초기 손패를 나눠준다. 좌석의 Health 를 보고 장수를 정하므로 역할 배정 뒤여야 한다.
	void DealInitialHands();

	//	첫 턴을 연다(FirstTurn 단계의 끝). 여기서 비로소 밧줄이 점화된다.
	void BeginFirstTurn();

	UPROPERTY(ReplicatedUsing = OnRep_IntroStage)
	EBGIntroStage IntroStage = EBGIntroStage::None;

	//	서버 전용. 초기 손패를 이미 나눴는가 — 건너뛰기가 배분을 두 번 부르는 것을 막는다.
	bool bInitialHandsDealt = false;

	//	복제 재전송으로 같은 통지가 두 번 나가는 것을 막는다(BroadcastedCinematicStage 와 같은 이유).
	EBGIntroStage BroadcastedIntroStage = EBGIntroStage::None;

	//	타이머는 서버에만 하나 돈다. 클라가 각자 세면 늦게 들어온 사람이 어긋난다.
	FTimerHandle IntroStageTimer;

	/**
	 * PvE 에서도 도입 연출을 재생할지.
	 *
	 * 기본은 끔이다 — 도입 연출은 PvP 한 판의 서두이고, PvE 는 조우를 연달아 도는 구조라
	 * 매 조우마다 붙으면 진행이 끊긴다. **혼자 연출을 확인할 때만** 켜서 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Intro")
	bool bPlayIntroInPvE = false;

	//	단계별 길이(초). 0 이하면 그 단계를 건너뛴다 — 전부 0 이면 도입 연출이 통째로 꺼진다.
	//	야영지 2 · 역할 4 · 용사 1 · 첫 턴 1 = 8초. 용사·첫 턴은 둘 다 보안관만 비춰 기획(2·2)의 반으로 줄였다.

	UPROPERTY(EditDefaultsOnly, Category = "BG|Intro", meta = (ClampMin = "0.0", Units = "s"))
	float IntroCampfireSeconds = 2.f;

	/** 역할 카드를 보여주는 시간. 이 시간이 지나면 카드가 퇴장하고 다음 단계로 넘어간다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Intro", meta = (ClampMin = "0.0", Units = "s"))
	float IntroRoleRevealSeconds = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Intro", meta = (ClampMin = "0.0", Units = "s"))
	float IntroHeroRevealSeconds = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Intro", meta = (ClampMin = "0.0", Units = "s"))
	float IntroFirstTurnSeconds = 1.f;

	/**
	 * 손패 상한. 이 장수를 넘기는 드로우는 넘는 만큼만 뽑히지 않는다.
	 *
	 * 뱅 원작의 "턴 종료 시 체력만큼만 남긴다" 를 대체한 규칙이다 — 버리기를 강요하지 않고
	 * 드로우 쪽에서 막는다. 0 이하면 상한을 두지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Turn", meta = (ClampMin = "0"))
	int32 MaxHandSize = 10;

	//	판이 진행 중인가. 무한 루프 방지 겸 턴 검증의 첫 조건이다.
	//	주의: 반드시 복제할 것 — CanSeatPlayCards / CanSeatDiscard 를 클라도 호출한다
	// (손패의 bPlayable 계산과 플레이 존 드롭 판정이 여기 달려 있다).
	// 복제하지 않으면 클라에서 항상 false 라 자기 차례에도 카드가 나가지 않는다.
	UPROPERTY(ReplicatedUsing = OnRep_TurnState)
	bool bMatchRunning = false;
	
	//	주의: 덱 셔플 전용 스트림이다. 주사위 판정은 UBGDiceComponent 가 별도 스트림을 쓴다
	// (한 스트림을 공유하면 판정 횟수가 셔플 결과를 밀어 재현이 깨진다). Player 진영 덱을 섞는다.
	UPROPERTY()
	FRandomStream RandomStream;

	//	PvE 전용. Enemy 진영 덱 셔플 스트림, Player 것과 분리한다(하나를 공유하면 적이 뽑는
	//	횟수가 플레이어 셔플 결과를 밀어 재현이 깨진다).
	UPROPERTY()
	FRandomStream EnemyRandomStream;
	//	시드 보존을 위해 한번 초기화 하면 다시는 초기화 하지 않는다.
	//	게임 초기화시에만 false로 되돌릴 것.
	UPROPERTY()
	bool bInitialized = false;

	//	확률 판정 전담 컴포넌트. 매치 시드로 InitializeDeck 에서 함께 초기화된다.
	UPROPERTY(VisibleAnywhere, Category = "BG|Dice")
	TObjectPtr<UBGDiceComponent> Dice;

	//	턴 제한 시간(밧줄) 전담. 턴을 여닫는 곳에서만 건드린다.
	UPROPERTY(VisibleAnywhere, Category = "BG|Rope")
	TObjectPtr<class UBGRopeComponent> Rope;

	/**
	 * 주사위/판정 UI를 보여준 뒤 월드 GameplayCue를 시작하기까지의 시간.
	 * 판정 게이지를 쓰면 카드 대기·퇴장 + 게이지 전체 길이에 맞춘다
	 * (UBGRollGaugeWidget 이 넘치면 경고를 남긴다).
	 * 이 시간 동안 카드 사용과 턴 종료를 막고 밧줄도 멈춘다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Cinematic", meta = (ClampMin = "0.0", Units = "s"))
	float ResolutionPresentationSeconds = 2.5f;

	/**
	 * 판정 연출 전에 사용한 카드를 중앙에 보여 주는 시간. 남의 카드는 판정 순간에야 처음 날아오므로
	 * 이게 없으면 무슨 카드인지 읽기도 전에 연출이 터진다.
	 * ⚠️ 카드 위젯과 서버 판정 대기에 **같이** 들어간다 — 한쪽만 늘리면 게이지 착지 전에 월드 연출이 터진다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Cinematic", meta = (ClampMin = "0.0", Units = "s"))
	float CardRevealSeconds = 1.5f;

public:
	UFUNCTION(BlueprintPure, Category = "BG|Rope")
	UBGRopeComponent* GetRope() const { return Rope; }
};

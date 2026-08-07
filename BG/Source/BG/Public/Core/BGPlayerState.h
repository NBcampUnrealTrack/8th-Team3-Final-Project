// BANG! 플레이어 상태 — 좌석/손패/장비/생존 등 플레이어별 데이터의 소유자.
// 손패는 COND_OwnerOnly 로 본인에게만, 장수는 전원에게 복제한다.
// 아직 카드 계층이 없어 좌석만 들고 있다 — 손패/장비는 M2 에서 여기에 붙인다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "Card/BGCardType.h"      // FBGCardInstance / FBGPendingStatus
#include "ActiveGameplayEffectHandle.h"
#include "Templates/SubclassOf.h"
#include "BGPlayerState.generated.h"

struct FBGCardInstance;
class UGameplayEffect;
class UBGReadyComponent;

/** 손패 내용이 바뀌었다. UI 는 여기에 구독해 갱신한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGHandChanged);

/**
 * 이탈/복귀 표시가 바뀌었다. 좌석 위 아이콘이나 이름표 흐리기 등에 쓴다.
 * 누가 튕길지 모르므로 정적 이벤트로 모은다 — 구독자는 좌석 번호로 대상을 찾는다.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBGDisconnectChanged,
	int32 /*Seat*/, bool /*bDisconnected*/);

/**
 * 남에게 보이는 정보(손패 장수·장비)가 바뀌었다. 머리 위 이름표가 이걸 보고 갱신한다.
 * 이탈 통지와 같은 이유로 정적이다 — 구독자가 좌석마다 다시 붙지 않아도 된다.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBGSeatPublicInfoChanged, int32 /*Seat*/);

UCLASS()
class BG_API ABGPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ABGPlayerState();

	/**
	 * 손패 변경 알림.
	 *
	 * 주의: 리슨서버 호스트는 자기 PlayerState 에 OnRep 이 호출되지 않는다(서버는 복제를 받지 않으므로).
	 * 그래서 AddCardToHand / RemoveCardFromHand 안에서도 같이 브로드캐스트한다.
	 * 이걸 놓치면 "클라는 손패가 보이는데 호스트만 안 보인다" 가 된다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "BG|Player")
	FOnBGHandChanged OnHandChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 좌석 번호(0-based). 거리 계산과 대상 지정의 기준. 미배정이면 INDEX_NONE.
	UFUNCTION(BlueprintPure, Category = "BG|Player")
	int32 GetSeatIndex() const { return SeatIndex; }

	// 서버 전용. 좌석의 소유자는 UBGSeatComponent 다 — 다른 곳에서 부르지 말 것.
	void SetSeatIndex(int32 InSeat);


	//	서버 전용. 손패 변경. HandCount 동기화를 한곳에 모으기 위해 Hand 는 이 둘로만 건드린다.
	void AddCardToHand(const FBGCardInstance& Card);
	bool RemoveCardFromHand(int32 InstanceId, FBGCardInstance& OutRemoved);

	//	주의: COND_OwnerOnly 라 "본인 클라"와 "서버"에서만 내용이 차 있다.
	// 다른 플레이어의 PlayerState 에서 부르면 클라에서는 항상 빈 배열이어야 정상이다
	// (비어 있지 않다면 은닉 정보가 새고 있는 것).
	const TArray<FBGCardInstance>& GetHand() const { return Hand; }

	//	전원에게 복제되는 공개 장수. 남의 손패 크기는 이 값으로만 알 수 있다.
	UFUNCTION(BlueprintPure, Category = "BG|Player")
	int32 GetHandCount() const { return HandCount; }

	//	장착 중인 파란 카드. 공개 정보라 전원에게 복제된다.
	const TArray<FBGCardInstance>& GetEquipment() const { return Equipment; }

	// ── 장비(파란 카드) 장착 ──
	//
	// 장비는 지속 효과다 — EquipEffect(Infinite GE)를 ASC 에 붙여 어트리뷰트를 바꾼다.
	// 주의: GE 핸들 생명주기를 Equipment 배열과 같은 곳에서 관리해야 한다.
	// 배열만 비우고 GE 를 남기면 "장비는 없는데 효과는 계속" 이 된다(사망 처리에서 실제로 발생).

	UFUNCTION(BlueprintPure, Category = "BG|Equip")
	bool HasEquippedCard(FName CardId) const;

	/** 서버 전용. 같은 종류를 이미 장착했으면 false(중복 장착 금지). */
	bool EquipCard(const FBGCardInstance& Card, TSubclassOf<UGameplayEffect> EquipEffect);

	/** 서버 전용. 장착을 해제하고 카드를 돌려준다. GE 도 함께 제거된다. */
	bool UnequipCard(FName CardId, FBGCardInstance& OutRemoved);

	// ── 턴당 카드 사용 (GDD §5.2 / §7.1) ──
	//
	// 한도는 어트리뷰트 CardUseLimit 이 갖고, "이번 턴에 몇 장 썼는가" 만 여기서 센다.
	// 턴 시작(BeginTurn)에 0 으로 초기화된다.
	//   주의: 버리기는 카드 사용이 아니다 — Discard 페이즈에서는 증가시키지 않는다.
	UFUNCTION(BlueprintPure, Category = "BG|Player")
	int32 GetCardsUsedThisTurn() const { return CardsUsedThisTurn; }

	//	서버 전용.
	void ResetCardsUsedThisTurn();
	void IncrementCardsUsedThisTurn();

	/**
	 * 사용 횟수를 되돌린다 ("카드 사용한도 회복" 효과 / 발동 실패 롤백).
	 * 0 미만으로는 내려가지 않는다 — 이번 턴에 쓴 만큼까지만 회복된다.
	 * 실제로 되돌린 횟수를 반환한다.
	 */
	int32 DecrementCardsUsedThisTurn(int32 Count = 1);

	// ── "다음 1회" 대기 상태 (Status.*) ──
	//
	// 약점 포착 / 함정 / 준비 / 대비가 여기에 쌓인다. 발동되면 소모된다.
	// 중첩은 허용하지 않는다 — 이미 가진 상태를 주는 카드는 사용 자체가 거부된다.

	UFUNCTION(BlueprintPure, Category = "BG|Status")
	bool HasPendingStatus(FGameplayTag StatusTag) const;

	/** 없으면 0. 약점 포착의 피해 증가량처럼 수치가 있는 상태에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Status")
	int32 GetPendingStatusAmount(FGameplayTag StatusTag) const;

	UFUNCTION(BlueprintPure, Category = "BG|Status")
	const TArray<FBGPendingStatus>& GetPendingStatuses() const { return PendingStatuses; }

	/** 서버 전용. 이미 같은 태그가 있으면 아무것도 하지 않고 false (중첩 금지). */
	bool AddPendingStatus(FGameplayTag StatusTag, int32 Amount);

	/** 서버 전용. 있으면 수치를 돌려주고 제거한다(소모). 없으면 false. */
	bool ConsumePendingStatus(FGameplayTag StatusTag, int32& OutAmount);

	/** 서버 전용. 전부 지운다(사망 등). */
	void ClearPendingStatuses();

	// ── 역할 공개 ──
	//
	// 진짜 역할은 ABGCharacter::RoleTag 에 있고 COND_OwnerOnly 로 본인에게만 간다.
	// 여기 값은 "공개된 역할" 만 담는다 — 보안관(판 시작 시), 사망자, 판 종료 시 전원.
	// 비공개면 무효 태그다. UI 는 반드시 이 값을 봐야 한다.
	UFUNCTION(BlueprintPure, Category = "BG|Player")
	FGameplayTag GetPublicRole() const { return PublicRoleTag; }

	//	서버 전용. 역할을 공개 상태로 올린다.
	void SetPublicRole(const FGameplayTag& InRole);

	// ── 생사 ──
	//
	// 주의: ASC 의 State.Dead 루즈 태그는 복제되지 않으므로 클라가 볼 수 없다.
	// "죽었다" 는 공개 사실이므로 여기에 복제 변수로 둔다.
	// (루즈 태그는 서버에서 GA 의 ActivationBlockedTags 를 막는 용도로만 쓴다)
	UFUNCTION(BlueprintPure, Category = "BG|Player")
	bool IsDead() const { return bIsDead; }

	//	서버 전용. 사망 확정 시 한 번만 호출된다.
	void SetDead(bool bInDead);

	/**
	 * 서버 전용. 손패와 장비를 전부 비우고 그 카드들을 돌려준다(사망 처리용).
	 * 호출자가 버린 패로 보내야 한다.
	 */
	void TakeAllCards(TArray<FBGCardInstance>& OutCards);

	// ── 이탈과 복귀 (밧줄 시스템) ─────────────────────────────────
	//
	// 판 중 이탈에도 이 PlayerState 는 파괴하지 않는다. 좌석·역할·손패가 여기 있어서,
	// 명단에서 빠지면 보안관이 튕긴 순간 무법자 승리가 나 버린다.

	/** 접속이 끊긴 채 자리만 남아 있는가. 사망과는 다르다 — 아직 살아 있는 좌석이다. */
	UFUNCTION(BlueprintPure, Category = "BG|Player")
	bool IsDisconnected() const { return bDisconnected; }

	//	서버 전용.
	void SetDisconnected(bool bInDisconnected);

	// 누군가의 이탈 표시가 바뀌었다. 서버·클라 양쪽에서 발사된다.
	// 위젯이 늦게 생기면 통지를 놓치므로, 구독 직후 IsDisconnected() 로 한 번 맞출 것.
	static FOnBGDisconnectChanged OnAnyDisconnectChanged;

	// 누군가의 공개 정보(손패 장수·장비)가 바뀌었다. 서버·클라 양쪽에서 발사된다.
	// 손패 "내용" 은 COND_OwnerOnly 라 남에게 안 간다 — 여기 실리는 것은 장수뿐이다.
	static FOnBGSeatPublicInfoChanged OnAnySeatPublicInfoChanged;

	/** 이 사람 턴에서 밧줄이 끝까지 탄 횟수. 스스로 턴을 끝내거나 돌아오면 0 으로 돌아간다. */
	UFUNCTION(BlueprintPure, Category = "BG|Player")
	int32 GetRopeStrikes() const { return RopeStrikes; }

	//	서버 전용. 누적값을 반환한다.
	int32 AddRopeStrike();
	void  ResetRopeStrikes();

	// ── 역할·직업 사본 (서버 전용, 복제 금지 — 역할은 은닉 정보다) ──
	//
	// 정본은 폰에 있다. 이탈로 폰이 사라지면 GetSeatRole 이 무효 태그를 돌려줘
	// 승패 판정이 틀어지므로, 배정 때 남긴 이 사본으로 답한다.
	void SetCachedRole(const FGameplayTag& InRole) { CachedRoleTag = InRole; }
	void SetCachedJob(const FGameplayTag& InJob)   { CachedJobTag  = InJob;  }

	FGameplayTag GetCachedRole() const { return CachedRoleTag; }
	FGameplayTag GetCachedJob()  const { return CachedJobTag;  }

	// 접속할 때 클라가 보낸 재접속 열쇠(?BGKey=). 서버 전용 — 알려지면 자리를 가로챌 수 있다.
	void SetPlayerKey(const FString& InKey) { PlayerKey = InKey; }
	const FString& GetPlayerKey() const { return PlayerKey; }

	// ── 체력·장비 스냅샷 (이탈 → 복귀) ────────────────────────────
	//
	// 이탈하면 폰이 파괴되어 체력과 장비 GE 가 함께 사라진다. 그대로 두면 체력 1 로 튕긴
	// 사람이 만피로 돌아오므로, 나갈 때 적어 두고 새 폰에 다시 얹는다.

	// 서버 전용. 이탈 직전 체력을 적고, 두고 가는 폰을 기억한다.
	void SaveVitals(APawn* LeavingPawn);

	// 서버 전용. 새로 스폰된 폰에 장비 GE 를 다시 붙이고 체력을 되돌린다.
	void RestoreVitals();

	// 두고 간 폰을 그대로 다시 물었을 때. 스냅샷을 쓰면 이중 적용이라 버린다.
	void DiscardSavedVitals() { SavedHealth = 0.f; }

	// 이탈하며 두고 간 폰. UnPossess 만 됐을 뿐 월드에는 그대로 남아 있다 —
	// 이 포인터를 놓치면 복귀 때 같은 자리에 폰이 하나 더 스폰되어 두 개가 겹친다.
	APawn* GetAbandonedPawn() const { return AbandonedPawn.Get(); }
	void   ClearAbandonedPawn() { AbandonedPawn = nullptr; }

	// 재접속한 컨트롤러의 새 PlayerState 로 판 상태를 넘긴다(복귀 처리가 유일한 호출자).
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

private:
	//	로비 준비 상태는 이 컴포넌트가 갖는다.
	UPROPERTY(VisibleAnywhere, Category = "BG|Player")
	TObjectPtr<UBGReadyComponent> Ready;

	//	소유 클라에 Hand 가 복제되면 호출된다.
	//	TODO(3.2): 여기서 OnHandChanged 를 브로드캐스트해 손패 UI 를 갱신한다.
	//	주의: 리슨서버 호스트는 OnRep 이 호출되지 않으므로, 그때는 AddCardToHand /
	// RemoveCardFromHand 안에서도 같은 알림을 쏴야 한다.
	UFUNCTION()
	void OnRep_Hand();

	UPROPERTY(Replicated)
	int32 SeatIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Hand)
	TArray<FBGCardInstance> Hand;       // COND_OwnerOnly

	//	전원 공개. 남의 이름표가 이 둘을 보고 갱신되므로 OnRep 으로 통지까지 한다.
	UPROPERTY(ReplicatedUsing = OnRep_PublicInfo)
	int32 HandCount = 0;
	UPROPERTY(ReplicatedUsing = OnRep_PublicInfo)
	TArray<FBGCardInstance> Equipment;   // 파란 카드

	//	둘 중 무엇이 와도 하는 일이 같아 OnRep 을 공유한다.
	UFUNCTION()
	void OnRep_PublicInfo();

	//	리슨서버 호스트는 자기 OnRep 을 받지 못한다 — 값을 바꾼 자리에서 직접 부른다.
	void BroadcastPublicInfoChanged() const;

	UPROPERTY(Replicated)
	bool bIsDead = false;                  // 전원 공개

	//	전원 공개 — 좌석 UI 가 "접속 끊김" 을 표시하고, 그 자리를 기다린다는 것을 모두가 알아야 한다.
	UPROPERTY(ReplicatedUsing = OnRep_Disconnected)
	bool bDisconnected = false;

	UFUNCTION()
	void OnRep_Disconnected();

	//	전원 공개 — 밧줄이 몇 번 탔는지는 남은 기회를 보여 주는 공개 정보다.
	UPROPERTY(Replicated)
	int32 RopeStrikes = 0;

	//	서버 전용 사본. 폰이 사라져도 역할·직업을 잃지 않기 위한 것 — 복제 금지(은닉 정보).
	UPROPERTY()
	FGameplayTag CachedRoleTag;
	UPROPERTY()
	FGameplayTag CachedJobTag;

	//	재접속 열쇠. 서버 전용 — 복제하면 남의 자리를 가로챌 수 있다.
	UPROPERTY()
	FString PlayerKey;

	//	이탈 직전 체력. 0 이하면 적어 둔 값이 없다는 뜻이다.
	UPROPERTY()
	float SavedHealth = 0.f;

	//	두고 간 폰. PS->GetPawn() 은 UnPossess 와 함께 비므로 따로 들고 있어야 한다.
	TWeakObjectPtr<APawn> AbandonedPawn;

	UPROPERTY(Replicated)
	FGameplayTag PublicRoleTag;            // 전원 공개 (공개된 역할만)

	UPROPERTY(Replicated)
	int32 CardsUsedThisTurn = 0;           // 전원 공개 (턴 UI 표시용)

	//	전원 공개 — 상대가 어떤 상태를 걸고 있는지는 공개 정보다(함정을 걸었다는 사실 등).
	UPROPERTY(Replicated)
	TArray<FBGPendingStatus> PendingStatuses;

	//	장비 GE 핸들. 서버 전용 — 복제하지 않는다(클라는 Equipment 배열만 보면 된다).
	TMap<FName, FActiveGameplayEffectHandle> EquipEffectHandles;

	/** 이 PlayerState 가 조종하는 폰의 ASC. 없으면 nullptr. */
	class UAbilitySystemComponent* GetOwnedAbilitySystemComponent() const;

};

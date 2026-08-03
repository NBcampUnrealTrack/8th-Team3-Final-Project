// BANG! 플레이어 컨트롤러 — 클라 입력을 서버로 중계한다 (md §1.2).
// HUD 는 GAS 인풋 바인딩을 쓰지 않고, 카드 사용 의도를 이 커스텀 RPC 로 보낸다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "UI/View/BGCardView.h"       // FBGCardView (OnCardPlayRequested 콜백 시그니처)
#include "UI/View/BGSeatView.h"       // FOnBGSeatSelected 등 델리게이트 타입
#include "BGPlayerController.generated.h"   // 항상 마지막

class UGameplayAbility;
class UAbilitySystemComponent;
class UBGHandWidget;
class UBGSeatBoardWidget;
class UBGTurnPanelWidget;
class ABGPlayerState;

UCLASS()
class BG_API ABGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 클라(소유): HUD 의 OnCardPlayRequested 에 바인딩한다. 서버로 중계만 한다.
	//   InstanceId : 사용할 카드 인스턴스 (FBGCardView.InstanceId). 손패의 특정 1장을 가리킨다.
	//   TargetSeat : 대상 좌석. 대상이 필요 없으면 INDEX_NONE.
	UFUNCTION(BlueprintCallable, Category = "BG")
	void RequestPlayCard(int32 InstanceId, int32 TargetSeat);

	// 테스트/디버그용 — 카드 종류 태그(Card.Id.Beer 등)로 사용 요청한다.
	//   손패에서 그 종류의 첫 장을 찾아 RequestPlayCard 로 넘긴다. 손패에 없으면 경고 후 무시.
	//   실제 UI 경로는 InstanceId 를 쓰므로(같은 카드 여러 장 구분) 이건 편의 함수다.
	UFUNCTION(BlueprintCallable, Category = "BG|Debug")
	void RequestPlayCardByTag(FGameplayTag CardId, int32 TargetSeat);

	// ── 손패 UI 배선  ────────────────────────────────────
	//
	// HUD(WBP_BGHUD)의 Event Construct 에서 자기 안의 손패 위젯을 한 번 넘겨주면,
	// 이후 갱신은 전부 여기서 처리한다. BP 쪽 작업은 이 노드 하나가 전부다.
	//
	// 위젯 생성과 PlayerState 복제 도착은 순서가 보장되지 않으므로, 양쪽 경로
	// (SetHandWidget / OnRep_PlayerState) 에서 모두 바인딩을 시도한다.
	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void SetHandWidget(UBGHandWidget* InHandWidget);

	// ── 좌석 보드 UI 배선 ────────────────────────────────────────
	//
	// HUD 가 자기 안의 좌석 보드를 한 번 넘겨준다. 이후 갱신은 여기서 처리한다.
	// 좌석 정보(HP/손패 장수/장비)는 바뀌는 경로가 여럿이라 단일 델리게이트가 없다.
	// 프로토타입에서는 타이머로 주기 갱신한다 — 좌석 위젯을 좌석 번호로 재사용하므로
	// 갱신 중에도 마우스 호버나 선택 상태가 끊기지 않는다.
	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void SetSeatBoardWidget(UBGSeatBoardWidget* InSeatBoard);

	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void RefreshSeatBoard();

	// ── 턴 패널 UI 배선 ──────────────────────────────────────────
	//
	// HUD 가 자기 안의 턴 패널을 한 번 넘겨준다. 좌석 보드와 같은 타이머로 함께 갱신된다.
	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void SetTurnPanelWidget(UBGTurnPanelWidget* InTurnPanel);

	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void RefreshTurnPanel();

	// ── 대상 좌석 선택 ───────────────────────────────────────────

	/**
	 * 대상 선택을 시작한다. 고를 수 있는 좌석은 호출자가 정한다.
	 * ContextId 는 응답에 그대로 실려 돌아온다(보통 카드 InstanceId).
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void BeginTargetSelection(int32 ContextId, const FText& Prompt, const TArray<int32>& SelectableSeats);

	/**
	 * 카드 사용용 편의 함수 — 살아 있는 다른 좌석 전부를 대상 후보로 연다.
	 * ⚠️ 사거리 판정이 아직 없어 실제 규칙보다 넓다. 최종 판정은 서버가 한다(STEP 7/8).
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void BeginTargetSelectionForCard(int32 CardInstanceId);

	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void CancelTargetSelection();

	// ── 턴 ───────────────────────────────────────────────────────
	//
	// HUD 의 "턴 종료" 버튼이 부른다. 손패가 한도(=현재 Health)를 넘으면 서버가
	// Discard 페이즈로 보내고, 그동안 카드를 중앙에 놓으면 사용 대신 버리기가 된다.
	UFUNCTION(BlueprintCallable, Category = "BG|Turn")
	void RequestEndTurn();

	/** 지금 내가 카드를 낼 수 있는가 (자기 턴 + Play 페이즈). UI 표시용. */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	bool IsMyTurnToPlay() const;

	/** 지금 내가 손패를 버려야 하는가 (자기 턴 + Discard 페이즈). */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	bool IsMyTurnToDiscard() const;

	/** 내 좌석 번호. PlayerState 가 없으면 INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	int32 GetMySeatIndex() const;

	/** 좌석이 확정됐다. 기본 동작으로 ServerRequestPlayCard 를 보낸다. */
	UPROPERTY(BlueprintAssignable, Category = "BG|Target")
	FOnBGSeatSelected OnTargetSeatSelected;

	UPROPERTY(BlueprintAssignable, Category = "BG|Target")
	FOnBGTargetSelectionCancelled OnTargetSelectionCancelled;

	// ── 콘솔 테스트 트리거 (구현: BGCardExec.cpp) ──────────────
	//
	// 손패는 창(=플레이어)마다 다르므로 PlayerController 에 둔다. GameMode 에 두면
	// 서버 창에서만 호출되어 "각 클라가 자기 손패만 보는지" 를 검증할 수 없다.
	UFUNCTION(Exec)
	void BG_DumpHand();

	// 턴 종료(UI 버튼 없이 테스트할 때). 자기 턴이 아니면 서버가 거부한다.
	UFUNCTION(Exec)
	void BG_EndTurn();

	// 현재 턴/페이즈/손패 한도를 출력한다.
	UFUNCTION(Exec)
	void BG_DumpTurn();

	/**
	 * 손패 각 카드의 4단계 확률을 기본값과 보정 후로 나란히 출력한다 (GDD §10).
	 *
	 * 카드 위젯에 확률 칸(Text_Chance)이 없어도 값이 맞는지 확인할 수 있다 —
	 * 표시가 안 되는 것과 계산이 틀린 것은 원인이 전혀 다르므로 분리해서 봐야 한다.
	 */
	UFUNCTION(Exec)
	void BG_DumpChances();

	// 손패 위젯을 현재 PlayerState 의 손패로 다시 그린다. 수동 갱신이 필요할 때 호출.
	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void RefreshHandWidget();
	
	virtual void OnRep_PlayerState() override;

protected:
	// PlayerState 의 OnHandChanged 에 구독한다. 위젯/PlayerState 중 늦게 오는 쪽에 맞춰 재시도된다.
	void TryBindHandDelegate();

	/**
	 * 판정 비율 보정 어트리뷰트(WeightBonus* / WeightMult*)의 변경을 구독한다.
	 *
	 * 손패 뷰의 최종 확률(FBGCardView::FinalChance)은 만들어질 때 값이 박히는 스냅샷이라
	 * 보정이 바뀌어도 저절로 갱신되지 않는다. OnHandChanged 만으로는 부족하다 —
	 * 장비를 장착하면 RemoveCardFromHand 가 GE 적용보다 **먼저** 손패 변경을 쏘기 때문에,
	 * 그때 다시 그린 확률은 장비가 반영되기 직전의 값이 된다.
	 * 확률을 틀리게 보여주면 플레이어를 속이는 것이므로 보정 쪽도 직접 구독한다.
	 */
	void TryBindAttributeDelegates();

	// 델리게이트 콜백. Dynamic 델리게이트라 UFUNCTION 이어야 한다.
	UFUNCTION()
	void HandleHandChanged();

	/** 보정 어트리뷰트가 바뀌었다 — 손패의 최종 확률을 다시 계산한다. */
	void HandleWeightAttributeChanged(const struct FOnAttributeChangeData& Data);

	/** 손패에서 카드를 중앙에 드롭했다. 대상이 필요한 카드면 좌석 선택으로 넘어간다. */
	UFUNCTION()
	void HandleCardPlayRequested(const FBGCardView& Card);

	/** 좌석 보드가 좌석 확정을 알려온다. */
	UFUNCTION()
	void HandleSeatSelected(int32 SeatIndex, int32 ContextId);

	UFUNCTION()
	void HandleTargetSelectionCancelled(int32 ContextId);

	/** 턴 패널의 턴 종료 버튼. */
	UFUNCTION()
	void HandleEndTurnRequested();

	/** UI 주기 갱신 타이머를 건다 (좌석 보드 / 턴 패널 / 손패 잠금). */
	virtual void BeginPlay() override;

	/** 우클릭 / ESC 로 대상 선택을 취소한다. */
	virtual void SetupInputComponent() override;

	/** 손패 드래그 잠금(저수준 설정). HandWidget 이 없으면 아무것도 하지 않는다. */
	void SetHandInteractionLocked(bool bLocked);

	/**
	 * 잠금 정책을 한곳에서 계산해 적용한다. 잠기는 조건은 둘이다:
	 *   1) 대상 좌석 선택 대기 중 — 두 번째 카드를 던지지 못하게
	 *   2) 내 차례가 아님(Play/Discard 페이즈가 아님)
	 *
	 * 좌석 보드와 같은 타이머로 주기 호출된다. 카드 위젯을 재생성하지 않으므로
	 * 갱신 중에도 드래그가 끊기지 않는다.
	 */
	void UpdateHandInteractionLock();

	/** 좌석 정보 주기 갱신 간격(초). 0 이하면 갱신하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|UI")
	float SeatBoardRefreshInterval = 0.25f;
	// 클라 → 서버. 검증 후 카드 사용을 처리한다.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestPlayCard(int32 InstanceId, int32 TargetSeat);

	// 서버: 검증 통과 후 실제 처리. 카드가 구동할 GA 를 부여+발동한다.
	void HandlePlayCard(int32 InstanceId, int32 TargetSeat);

	UFUNCTION(Server, Reliable)
	void ServerRequestEndTurn();

	// 클라 → 서버. Discard 페이즈에서 손패를 한 장 버린다.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestDiscardCard(int32 InstanceId);

	// 서버: 사용이 성사된 카드를 손패에서 빼 버린 패로 보낸다.
	//   뱅 규칙상 낸 카드는 결과와 무관하게 버려진다(뱅!이 빗나가도 카드는 돌아오지 않는다).
	//   단 GA 발동 자체가 실패하면 호출하지 않는다 — 카드가 조용히 사라지면 추적이 어렵다.
	void ConsumeCard(int32 InstanceId);

	// 카드 종류(Card.Id.*) → 그 카드가 구동할 GA 매핑.
	//   ⚠️ 반드시 에디터(BP 컨트롤러 디폴트)에서 채운다. 비어 있으면 카드가 발동되지 않는다:
	//     Card.Id.Bang→GA_Bang, Card.Id.Beer→GA_Heal, Card.Id.Missed→GA_Missed, ...
	//   키는 RequestPlayCard 로 넘어온 CardId 태그와 그대로 매칭된다.
	UPROPERTY(EditDefaultsOnly, Category = "BG")
	TMap<FGameplayTag, TSubclassOf<UGameplayAbility>> AbilityByCardId;

	// TargetSeat → 그 좌석 플레이어의 폰. 대상이 필요 없는 카드면 INDEX_NONE → nullptr.
	AActor* ResolveTargetActor(int32 TargetSeat) const;

private:
	// HUD 가 넘겨준 손패 위젯. 소유는 HUD 에 있고 여기서는 참조만 한다.
	UPROPERTY()
	TObjectPtr<UBGHandWidget> HandWidget;

	// 중복 구독을 막기 위해 현재 구독 중인 PlayerState 를 기억한다.
	UPROPERTY()
	TWeakObjectPtr<ABGPlayerState> BoundPlayerState;

	//	같은 이유로 구독 중인 ASC 를 기억한다. 폰이 늦게 오거나 갈아끼워지면 다시 건다.
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	// HUD 가 넘겨준 좌석 보드. 소유는 HUD 에 있고 여기서는 참조만 한다.
	UPROPERTY()
	TObjectPtr<UBGSeatBoardWidget> SeatBoardWidget;

	UPROPERTY()
	TObjectPtr<UBGTurnPanelWidget> TurnPanelWidget;

	FTimerHandle SeatRefreshTimer;
};

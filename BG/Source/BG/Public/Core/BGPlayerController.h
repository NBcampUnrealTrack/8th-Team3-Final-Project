// BANG! 플레이어 컨트롤러 — 클라 입력을 서버로 중계한다 (md §1.2).
// HUD 는 GAS 인풋 바인딩을 쓰지 않고, 카드 사용 의도를 이 커스텀 RPC 로 보낸다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "UI/View/BGCardView.h"       // FBGCardView (OnCardPlayRequested 콜백 시그니처)
#include "UI/View/BGSeatView.h"       // FOnBGSeatSelected 등 델리게이트 타입
#include "UI/View/BGResolutionView.h" // EBGResolutionTier (ToResolutionTier 반환형)
#include "Card/BGGameDataTypes.h"     // EBGDiceOutcome (ToResolutionTier 인자)
#include "UI/Target/BGGameplayHudOwner.h"   // 조준 중 클릭 통과 대상 알려주기
#include "BGPlayerController.generated.h"   // 항상 마지막

class UGameplayAbility;
class UAbilitySystemComponent;
class UBGHandWidget;
class UBGSeatBoardWidget;
class UBGTurnPanelWidget;
class UBGResolutionWidget;
class ABGPlayerState;

UCLASS()
class BG_API ABGPlayerController : public APlayerController, public IBGGameplayHudOwner
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

	/**
	 * 판정 결과 위젯을 등록한다. HUD 가 만들면서 넘긴다.
	 * 등록하지 않아도 게임은 그대로 돌아간다 — 결과가 화면에 안 뜰 뿐이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void SetResolutionWidget(UBGResolutionWidget* InResolutionWidget);

	UFUNCTION(BlueprintCallable, Category = "BG|UI")
	void RefreshTurnPanel();

	ABGPlayerController();

	// ── 대상 좌석 선택 ───────────────────────────────────────────

	/**
	 * 대상 선택을 시작한다. 고를 수 있는 좌석은 호출자가 정한다.
	 * ContextId 는 응답에 그대로 실려 돌아온다(보통 카드 InstanceId).
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void BeginTargetSelection(int32 ContextId, const FText& Prompt, const TArray<int32>& SelectableSeats);

	/**
	 * 카드 사용용 편의 함수 — 살아 있는 다른 좌석 전부를 대상 후보로 연다.
	 * 주의: 사거리 판정이 아직 없어 실제 규칙보다 넓다. 최종 판정은 서버가 한다(STEP 7/8).
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void BeginTargetSelectionForCard(int32 CardInstanceId);

	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void CancelTargetSelection();

	/** 지금 대상을 고르는 중인가. 어느 표현(좌석 보드/월드 조준)이든 하나라도 켜져 있으면 true. */
	UFUNCTION(BlueprintPure, Category = "BG|Target")
	bool IsSelectingTarget() const;

	/**
	 * 화면 위쪽 2D 좌석 보드를 띄울지. 머리 위 3D 정보와 같은 것을 두 번 보여주게 되어
	 * 지금은 꺼 둔다. 켜면 예전처럼 다시 나온다 — HUD 애셋은 건드리지 않았다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Target")
	bool bShowSeatBoardWidget = false;

	//	IBGGameplayHudOwner — 조준 중 클릭을 통과시킬 위젯을 조준 컴포넌트에 알려 준다.
	virtual void CollectGameplayHudWidgets(TArray<UWidget*>& OutWidgets) const override;

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

	// 엔진 기본 동작은 PlayerState 파괴지만, 판 중에 파괴하면 그 좌석의 역할·손패가 사라져
	// 승패 판정이 틀어진다. 자리를 비운 것으로 표시된 PlayerState 는 살려 둔다.
	virtual void CleanupPlayerState() override;

	// 컨트롤러가 사라질 때 폰도 함께 파괴된다. 판 중이면 먼저 떼어 내 월드에 남긴다 —
	// Logout 보다 이 시점이 빨라서, 여기서 잡지 않으면 캐릭터가 자리에서 사라진다.
	virtual void Destroyed() override;

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

	/**
	 * GameState 의 판정 통지를 구독한다. GameState 는 클라에 늦게 도착하므로
	 * 손패 델리게이트와 마찬가지로 여러 진입점에서 재시도한다.
	 */
	void TryBindResolutionDelegate();

	/** 판정 결과가 도착했다. 게임 로직 타입 → UI 뷰모델 변환이 여기서 일어난다. */
	void HandleCardResolved(const struct FBGCardResolution& Resolution);

	// GameState 의 턴 상태 통지를 구독한다. 늦게 도착하므로 여러 진입점에서 재시도한다.
	void TryBindTurnStateDelegate();

	/** 페이즈·현재 좌석이 바뀌었다. 턴 패널과 좌석 보드를 다시 그린다. */
	UFUNCTION()
	void HandleTurnStateChanged();

	// 내 차례가 아니게 됐다. 진행 중이던 대상 선택을 접고 중앙에 잡혀 있던 카드를 손패로 되돌린다.
	void AbortActionIfTurnLost();


	/** EBGDiceOutcome → EBGResolutionTier. 뷰 계층이 게임 enum 을 모르게 하는 경계다. */
	static EBGResolutionTier ToResolutionTier(EBGDiceOutcome Outcome);

	// ── 연출 카메라 ────────────────────────────────────────────────

	/** GameState 의 샷 상태를 구독한다. GameState 가 늦게 오므로 재시도된다. */
	void TryBindCameraShotDelegate();

	/** 샷이 바뀌었다 — 뷰타깃과 로컬 카메라 액터를 갱신한다. */
	void HandleCameraShotChanged(const struct FBGCameraShot& Shot);

	/**
	 * 연출 카메라 액터를 로컬에 스폰한다(로컬 컨트롤러 전용, 1회).
	 * 복제하지 않는다 — 클라마다 자기 것을 갖고 자기 화면만 책임진다.
	 */
	void EnsureCinematicCamera();

	//	연출 속도 설정은 ABGGameState 에 모여 있다 (진입·단계·종료가 하나의 체감이라
	//	한곳에서 조율해야 한다). 여기서 따로 들지 않고 그때그때 읽어 쓴다.

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

	/**
	 * 갱신 안전망 간격(초). 0 이하면 끈다.
	 *
	 * 턴 UI 는 GameState 의 OnTurnStateChanged 로 갱신된다 — 이 타이머는 체력처럼
	 * 통지가 없는 값과 늦게 도착하는 구독 재시도만 맡는다. 그래서 주기가 느려도 된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|UI")
	float SeatBoardRefreshInterval = 1.0f;
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
	//   주의: 반드시 에디터(BP 컨트롤러 디폴트)에서 채운다. 비어 있으면 카드가 발동되지 않는다:
	// Card.Id.Bang→GA_Bang, Card.Id.Beer→GA_Heal, Card.Id.Missed→GA_Missed, ...
	// 키는 RequestPlayCard 로 넘어온 CardId 태그와 그대로 매칭된다.
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

	//	판정 결과 표시. 없어도 게임은 돌아간다(결과가 화면에 안 뜰 뿐).
	UPROPERTY()
	TObjectPtr<UBGResolutionWidget> ResolutionWidget;

	//	중복 구독을 막기 위해 현재 구독 중인 GameState 를 기억한다.
	UPROPERTY()
	TWeakObjectPtr<class ABGGameState> BoundGameState;

	//	카메라 샷 구독은 판정 구독과 시점이 다를 수 있어 따로 기억한다.
	UPROPERTY()
	TWeakObjectPtr<class ABGGameState> BoundCameraGameState;

	//	턴 상태 통지를 구독한 GameState. 중복 구독을 막는다.
	TWeakObjectPtr<class ABGGameState> BoundTurnGameState;

	//	머리 위 좌석 정보 + 커서 호버 판정. 보는 사람 화면에만 있는 것이라 복제하지 않는다.
	UPROPERTY(VisibleAnywhere, Category = "BG|Target")
	TObjectPtr<class UBGSeatInfoDisplayComponent> SeatInfoDisplay;

	//	월드 캐릭터를 눌러 대상을 고르는 조준 모드.
	UPROPERTY(VisibleAnywhere, Category = "BG|Target")
	TObjectPtr<class UBGWorldTargetingComponent> WorldTargeting;

	//	로컬 연출 카메라. 복제하지 않는다 — 각 클라가 자기 것을 스폰한다.
	UPROPERTY()
	TObjectPtr<class ABGCinematicCamera> CinematicCamera;

	FTimerHandle SeatRefreshTimer;
};

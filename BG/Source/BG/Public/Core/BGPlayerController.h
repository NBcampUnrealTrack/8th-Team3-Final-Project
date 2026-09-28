// BANG! 플레이어 컨트롤러 — 클라 입력을 서버로 중계한다 (md §1.2).
// HUD 는 GAS 인풋 바인딩을 쓰지 않고, 카드 사용 의도를 이 커스텀 RPC 로 보낸다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "UI/View/BGCardView.h"       // FBGCardView (OnCardPlayRequested 콜백 시그니처)
#include "UI/View/BGSeatView.h"       // FOnBGSeatSelected 등 델리게이트 타입
#include "UI/View/BGResolutionView.h" // EBGResolutionTier (ToResolutionTier 반환형)
#include "UI/Presentation/BGPresentationTypes.h"  // FBGPresentationHandle (시네마틱 트랙)
#include "Card/BGGameDataTypes.h"     // EBGDiceOutcome (ToResolutionTier 인자)
#include "Core/BGIntroStage.h"        // EBGIntroStage — BP 델리게이트 인자라 완전한 타입이 필요하다
#include "UI/Target/BGGameplayHudOwner.h"   // 조준 중 클릭 통과 대상 알려주기
#include "BGPlayerController.generated.h"   // 항상 마지막

class UGameplayAbility;
class UAbilitySystemComponent;
class UBGHandWidget;
class UBGSeatBoardWidget;
class UBGTurnPanelWidget;
class UBGResolutionWidget;
class ABGPlayerState;
class UCameraShakeBase;

//	기반 타입을 적어 두면 전방 선언이 된다 — 연출 단계 하나 때문에 GameState 헤더 전체를
//	끌어오지 않는다(BGGameState.h 는 GAS·DT 까지 딸려 온다).
enum class EBGCinematicStage : uint8;

/**
 * 서버가 카드 사용을 거부한 사유.
 *
 * ⚠️ 거부는 **반드시** 클라에 통지해야 한다. 통지가 없으면 클라는 중앙에 올라간 카드를
 *    풀어 줄 신호를 영영 못 받고, 안전장치(HoldTimeoutSeconds)가 도는 30초 동안 카드가
 *    화면 한복판에 박혀 있는다. 중복 장착처럼 정상 플레이 중에 밟는 경로가 있다.
 */
UENUM(BlueprintType)
enum class EBGCardPlayRejectReason : uint8
{
	NotYourTurn        UMETA(DisplayName = "차례가 아님"),
	NoCardUsesLeft     UMETA(DisplayName = "카드 사용 한도 소진"),
	TargetDead         UMETA(DisplayName = "대상이 이미 사망"),
	CardNotInHand      UMETA(DisplayName = "손패에 없는 카드"),
	AlreadyEquipped    UMETA(DisplayName = "이미 장착 중"),
	StatusAlreadyOn    UMETA(DisplayName = "이미 같은 상태 — 중첩 불가"),
	InternalError      UMETA(DisplayName = "내부 오류")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBGCardPlayRejected,
	int32, InstanceId, EBGCardPlayRejectReason, Reason);

/** 도입 연출 단계가 바뀌었다 — HUD(WBP)가 자기 등장·퇴장 애니메이션을 여기에 건다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGIntroStageChangedBP, EBGIntroStage, Stage);

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

	/**
	 * HUD 가 넘겨준 손패 위젯. 아직 안 넘겼으면 nullptr.
	 * 몽타주를 재생하는 액터는 UI 를 모르므로, 연출 종료 통지가 여기를 거쳐 손패를 찾는다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|UI")
	UBGHandWidget* GetHandWidget() const { return HandWidget; }

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

	/** 좌하단 본인 정보도 이 패널이 맡는다. 좌석 정보 표시 컴포넌트가 값을 밀어넣는다. */
	UFUNCTION(BlueprintPure, Category = "BG|UI")
	UBGTurnPanelWidget* GetTurnPanelWidget() const { return TurnPanelWidget; }

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

	// PvE 로비의 "시작" 버튼이 부른다. PvP는 세션 로비(UBGSessionFlow::StartGame)를 쓰고,
	// PvE는 세션을 안 만들어서 그 UI가 없다. 여기가 PvE 전용 진입점이다.
	UFUNCTION(BlueprintCallable, Category = "BG|PvE")
	void RequestStartPvEMatch();

	// PvE 보상 선택 화면의 카드 버튼이 부른다. CardId 는 DT_BGCard 의 Row 이름.
	UFUNCTION(BlueprintCallable, Category = "BG|PvE")
	void RequestSelectPvEReward(FName CardId);

	/** 지금 내가 카드를 낼 수 있는가 (자기 턴 + Play 페이즈). UI 표시용. */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	bool IsMyTurnToPlay() const;

	/** 지금 내가 손패를 버려야 하는가 (자기 턴 + Discard 페이즈). */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	bool IsMyTurnToDiscard() const;

	/** 내 좌석 번호. PlayerState 가 없으면 INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	int32 GetMySeatIndex() const;

	// 서버가 아니라 각 클라이언트가 로컬로 "나는 이 판을 이겼나"를 판정한다.
	// OnMatchWon 이 넘겨주는 WinningRole 과 내 PlayerState 의 캐시된 역할을 비교한다.
	UFUNCTION(BlueprintPure, Category = "BG|Match")
	bool DidIWin(FGameplayTag WinningRole) const;

	/** 좌석이 확정됐다. 기본 동작으로 ServerRequestPlayCard 를 보낸다. */
	UPROPERTY(BlueprintAssignable, Category = "BG|Target")
	FOnBGSeatSelected OnTargetSeatSelected;

	UPROPERTY(BlueprintAssignable, Category = "BG|Target")
	FOnBGTargetSelectionCancelled OnTargetSelectionCancelled;

	// ── 도입 연출 ────────────────────────────────────────────────
	//
	//  HUD 는 BP 라서 GameState 의 네이티브 통지를 못 듣는다. 여기서 BP 용으로 한 번 더 쏜다.
	//  ⚠️ 무엇을 보여줄지는 HUD 가 정한다 — 코드는 "지금 몇 단계다" 만 알린다.

	/** 도입 연출 단계가 바뀌었다. HUD 가 Event Construct 에서 여기에 바인딩하면 된다. */
	UPROPERTY(BlueprintAssignable, Category = "BG|Intro")
	FOnBGIntroStageChangedBP OnIntroStageChangedBP;

	/**
	 * 지금 도입 연출 단계. HUD 가 늦게 만들어져 통지를 놓쳤을 때 스스로 맞추는 용도다.
	 * 판이 도는 중이면 None.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Intro")
	EBGIntroStage GetIntroStage() const;

	/** 첫 턴을 가져갈 좌석의 이름. ⑤ "에드릭의 차례" 문구에 쓴다. 아직 모르면 빈 텍스트. */
	UFUNCTION(BlueprintPure, Category = "BG|Intro")
	FText GetFirstTurnPlayerName() const;

	/** 용사(보안관)의 이름. ③ "용사는 에드릭입니다" 문구에 쓴다. 아직 모르면 빈 텍스트. */
	UFUNCTION(BlueprintPure, Category = "BG|Intro")
	FText GetHeroPlayerName() const;

	/** 첫 턴이 내 차례인가. ⑤ 에서 "내 차례" 와 "OO의 차례" 를 가르는 데 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Intro")
	bool IsFirstTurnMine() const;
	
	// ─────────────── 채팅 ───────────────────── //
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSendChatMessage(const FString& Message);

	// ─────────────── 핑 ───────────────────── //
	//	검증과 전파는 UBGPingComponent 가 맡는다. 여기는 RPC 경계만 제공한다.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSendPing(FGameplayTag PingTag, int32 TargetSeat);

	UFUNCTION(BlueprintPure, Category = "BG|Ping")
	class UBGPingComponent* GetPingComponent() const { return Ping; }
	
	UPROPERTY(EditDefaultsOnly, Category = "BG|UI")
	TSubclassOf<class UBGChatingWidget> ChatWidgetClass;

	UPROPERTY()
	class UBGChatingWidget* ChatWidgetInstance;

	// PvE 로비("게임 시작" 버튼)는 여기서 들고 있지 않다 — WBP_BGHUD 안에 얹혀 있고
	// 띄울지 말지는 UBGPvELobbyWidget 이 스스로 본다.

	// PvE 조우 승리 보상 선택 위젯. 보상 후보가 있을 때만 스스로 나타난다(PvELobbyWidget과 짝).
	UPROPERTY(EditDefaultsOnly, Category = "BG|PvE")
	TSubclassOf<class UBGRewardSelectWidget> RewardSelectWidgetClass;

	UPROPERTY()
	class UBGRewardSelectWidget* RewardSelectWidgetInstance;

	/**
	 * 서버가 카드 사용을 거부했다. 카드는 이미 손패로 되돌려 놓은 뒤에 발생한다 —
	 * 구독자는 사유를 보여 주기만 하면 된다(전용 UI 가 생기면 여기에 붙일 것).
	 */
	UPROPERTY(BlueprintAssignable, Category = "BG|Card")
	FOnBGCardPlayRejected OnCardPlayRejected;

	/** 거부 사유를 플레이어에게 보여 줄 문장으로. */
	UFUNCTION(BlueprintPure, Category = "BG|Card")
	static FText GetPlayRejectText(EBGCardPlayRejectReason Reason);

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

	// 도입 연출을 건너뛰고 바로 판을 연다. 서버가 단계를 소유하므로 전원이 함께 건너뛴다.
	UFUNCTION(Exec)
	void BG_SkipIntro();

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

	// 서버: 검증 통과 후 실제 처리. 카드가 구동할 GA 를 부여+발동한다.
	void HandlePlayCard(int32 InstanceId, int32 TargetSeat);

	/** 카드 연출 큐. HUD·손패·GameplayCue 가 여기에 구독하거나 신호를 넣는다. */
	UFUNCTION(BlueprintPure, Category = "BG|Presentation")
	class UBGPresentationDirector* GetPresentationDirector() const { return PresentationDirector; }

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
	 * GameState 의 통지(판정 · 장비 · 카메라 샷 · 연출 단계 · 턴)를 한 번에 구독한다.
	 * GameState 는 클라에 늦게 도착하므로 여러 진입점에서 재시도한다.
	 */
	void TryBindGameStateDelegates();

	/** 판정 결과가 도착했다. 게임 로직 타입 → UI 뷰모델 변환이 여기서 일어난다. */
	void HandleCardResolved(const struct FBGCardResolution& Resolution);

	/**
	 * 누군가 장비를 착용했다. 남의 착용이면 관전 카드를 중앙에 세운다.
	 * 판정이 없으므로 등급도 눈도 없다 — bResolvedWithoutRoll 로 표시해 넘긴다.
	 */
	void HandleCardEquipped(int32 SeatIndex, FGameplayTag CardId);

	/** 이 판의 GameState. 아직 복제되지 않았으면 nullptr — 판 초반에는 흔한 일이다. */
	class ABGGameState* GetBGGameState() const;

	/** 페이즈·현재 좌석이 바뀌었다. 턴 패널과 좌석 보드를 다시 그린다. */
	UFUNCTION()
	void HandleTurnStateChanged();

	// 내 차례가 아니게 됐다. 진행 중이던 대상 선택을 접고 중앙에 잡혀 있던 카드를 손패로 되돌린다.
	void AbortActionIfTurnLost();

	/** 연출 단계가 바뀌었다. 시네마틱 트랙의 표를 여기서 쥐고 놓는다. */
	void HandleCinematicStageChangedForFX(EBGCinematicStage Stage, class ABGCharacter* Subject);

	/** 도입 연출 단계가 바뀌었다. 화면 페이드와 역할 카드를 여기서 연다. */
	void HandleIntroStageChanged(EBGIntroStage Stage);

	/** 역할 카드를 이 단계에 맞게 띄우거나 내린다. */
	void UpdateIntroRoleCard(EBGIntroStage Stage);

	/**
	 * 검은 화면에 머무는 시간(초). 장작 소리만 들리는 구간이다.
	 * ⚠️ IntroFadeInSeconds 와 합쳐서 야영지 진입 단계 길이(기본 2초)를 넘지 않게 둘 것 —
	 *    넘으면 다음 단계가 시작된 뒤에도 화면이 덮여 있다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Intro", meta = (ClampMin = "0.0", Units = "s"))
	float IntroBlackHoldSeconds = 0.5f;

	/** 검은 화면이 걷히는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Intro", meta = (ClampMin = "0.0", Units = "s"))
	float IntroFadeInSeconds = 1.2f;

	/** 검은 화면에 머문 뒤 밝아지기 시작한다. */
	void BeginIntroFadeIn();

	/** 도입 ② 에서 띄울 내 역할 카드. 비워 두면 그 단계는 화면에 아무것도 뜨지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Intro")
	TSubclassOf<class UBGIntroRoleCardWidget> IntroRoleCardClass;

	UPROPERTY(Transient)
	TObjectPtr<class UBGIntroRoleCardWidget> IntroRoleCardInstance;

	/** 시네마틱이 도는 동안 쥐고 있는 표. */
	FBGPresentationTrackHold CinematicHold;

	/** 판정 UI 대기가 도는 동안 쥐고 있는 표. 턴 상태 통지마다 상태를 맞춘다. */
	void RefreshResolutionPresentationTrack();
	FBGPresentationTrackHold ResolutionHold;


	/** EBGDiceOutcome → EBGResolutionTier. 뷰 계층이 게임 enum 을 모르게 하는 경계다. */
	static EBGResolutionTier ToResolutionTier(EBGDiceOutcome Outcome);

	/**
	 * 판정이 확정된 순간 화면을 흔든다 — 등급과 효과 세기에서 강도를 유도한다.
	 *
	 * ⚠️ 좌석으로 거르지 말 것. 남이 낸 카드도 전원이 같은 판을 보고 있으므로 함께 흔들려야 한다.
	 *    (손패 연출은 내 카드만이지만 이쪽은 다르다 — HandleCardResolved 의 호출 위치 주석 참고.)
	 */
	void PlayResolveShake(const struct FBGCardPlayContext& Ctx);

	/**
	 * 판정 셰이크 애셋. 등급별로 나누지 않는다 — 강도는 StartCameraShake 의 Scale 로 준다.
	 * 비어 있으면 셰이크만 조용히 생략된다(연출 없이도 판정 자체는 정상 진행).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Camera")
	TSubclassOf<UCameraShakeBase> ResolveShakeClass;

	/**
	 * 판정 등급별 셰이크 배율. 인덱스는 EBGResolutionTier 순서 —
	 * None / 대실패 / 실패 / 성공 / 대성공.
	 *
	 * 대실패가 성공보다 큰 것은 의도다. 판이 뒤집히는 순간일수록 세게 흔들어야 읽힌다.
	 * None(장비처럼 판정하지 않는 카드)은 0 이라 흔들리지 않는다.
	 *
	 * ⚠️ 여기 기본값을 고쳐도 이미 저장된 BP 는 옛 배열을 들고 있다. 조정은 BP 디폴트에서 할 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Camera")
	TArray<float> TierShakeScale = { 0.f, 1.4f, 0.5f, 0.8f, 1.6f };

	/** 셰이크 강도 상한. 데이터 테이블 수치가 커져도 화면이 무너지지 않게 막는다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Camera", meta = (ClampMin = "0.0"))
	float MaxShakeScale = 3.f;

	// ── 연출 카메라 ────────────────────────────────────────────────

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

	/** 사용한 카드가 퇴장을 시작했다 — 판정 위젯에 전달한다(게이지의 시작 신호). */
	UFUNCTION()
	void HandleCardPlayExitStarted(int32 InstanceId);

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

	/**
	 * 서버 → 소유 클라. 사용이 거부됐으니 카드를 손패로 되돌리라고 알린다.
	 *
	 * ⚠️ HandlePlayCard 에서 **거부하며 빠져나가는 모든 return 은 이걸 거쳐야 한다.**
	 *    하나라도 빠지면 그 경로에서만 카드가 중앙에 30초 박히는, 재현 조건이 좁은
	 *    버그가 된다 — RejectPlayCard 를 통해서만 거부하도록 통일했다.
	 */
	UFUNCTION(Client, Reliable)
	void ClientPlayCardRejected(int32 InstanceId, EBGCardPlayRejectReason Reason);

	/** 서버: 거부를 기록하고 소유 클라에 통지한다. HandlePlayCard 의 유일한 거부 출구다. */
	void RejectPlayCard(int32 InstanceId, EBGCardPlayRejectReason Reason);

	UFUNCTION(Server, Reliable)
	void ServerRequestEndTurn();

	/** 도입 연출 건너뛰기(콘솔 BG_SkipIntro). 단계는 서버가 소유하므로 전원이 함께 건너뛴다. */
	UFUNCTION(Server, Reliable)
	void ServerRequestSkipIntro();

	UFUNCTION(Server, Reliable)
	void ServerRequestStartPvEMatch();

	UFUNCTION(Server, Reliable)
	void ServerRequestSelectPvEReward(FName CardId);

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
	/** 기존 공개 API는 Basic을 유지하고, 카드 경로만 DT의 연출 스타일을 함께 전달한다. */
	void BeginTargetSelectionWithStyle(int32 ContextId, const FText& Prompt,
		const TArray<int32>& SelectableSeats, EBGTargetSelectionStyle PresentationStyle);

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

	/** 지금 차례인 캐릭터에 CurrentTurn 스텐실을 찍는다. 내 차례면 아무도 찍지 않는다. */
	void RefreshTurnHighlight();

	//	직전에 CurrentTurn 을 찍어 둔 캐릭터. 다음 차례로 넘어갈 때 이 값부터 지운다.
	TWeakObjectPtr<class ABGCharacter> HighlightedTurnCharacter;

	//	머리 위 좌석 정보 + 커서 호버 판정. 보는 사람 화면에만 있는 것이라 복제하지 않는다.
	UPROPERTY(VisibleAnywhere, Category = "BG|Target")
	TObjectPtr<class UBGSeatInfoDisplayComponent> SeatInfoDisplay;

	//	월드 캐릭터를 눌러 대상을 고르는 조준 모드.
	UPROPERTY(VisibleAnywhere, Category = "BG|Target")
	TObjectPtr<class UBGWorldTargetingComponent> WorldTargeting;

	//	카드 연출의 순서를 정하는 곳. 연출 상태의 주인이라 판 내내 살아 있어야 한다 —
	//	카드 위젯이 대신 들면 카드가 죽을 때 연출도 함께 끊긴다.
	UPROPERTY(VisibleAnywhere, Category = "BG|Presentation")
	TObjectPtr<class UBGPresentationDirector> PresentationDirector;

	//	핑 발신·표시. 보는 사람 화면의 일이라 복제하지 않는다.
	UPROPERTY(VisibleAnywhere, Category = "BG|Ping")
	TObjectPtr<class UBGPingComponent> Ping;

	UPROPERTY(VisibleAnywhere, Category = "BG|Ping")
	TObjectPtr<class UBGPingDisplayComponent> PingDisplay;

	//	로컬 연출 카메라. 복제하지 않는다 — 각 클라가 자기 것을 스폰한다.
	UPROPERTY()
	TObjectPtr<class ABGCinematicCamera> CinematicCamera;

	FTimerHandle SeatRefreshTimer;

	//	검은 화면에 머무는 동안 도는 타이머. ⚠️ Destroyed 에서 반드시 끊는다.
	FTimerHandle IntroFadeTimer;
	
	void HandleChatKeyPressed();
};

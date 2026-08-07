// BANG! 밧줄 — 턴 제한 시간(하스스톤의 그 밧줄). 다 타면 턴이 자동으로 넘어간다.
// 자리를 비운 사람의 턴에서 판이 멈추는 것을 막는 워치독이다(Prototype-Workflow §7).
// 연속 MaxStrikes 번 타면 그 좌석을 죽여 기존 사망 경로로 승패를 처리한다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BGRopeComponent.generated.h"

class ABGGameState;

// 밧줄을 잡을 때 쓰는 이유 이름. 잡은 쪽과 놓는 쪽이 같은 이름을 써야 짝이 맞는다.
// 블루프린트에서는 같은 문자열을 FName 으로 넣으면 된다.
namespace BGRopePause
{
	inline const FName Cinematic (TEXT("Cinematic"));    // 카드 연출 재생 중 (GameState 가 자동으로 건다)
	inline const FName Resolution(TEXT("Resolution"));   // 주사위(판정) 표시 중 (GameState 가 자동으로 건다)
	inline const FName Sequence  (TEXT("Sequence"));     // 시퀀서·연출 스크립트가 직접 잡을 때
}

/** 지금 타고 있는 밧줄. 셋이 따로 복제되면 반쪽 값으로 UI 가 갱신되므로 한 덩어리로 묶는다. */
USTRUCT(BlueprintType)
struct FBGRopeState
{
	GENERATED_BODY()

	/** 밧줄이 걸린 좌석. INDEX_NONE 이면 타는 중이 아니다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Rope")
	int32 Seat = INDEX_NONE;

	/** 끝나는 서버 시각(GetServerWorldTimeSeconds 기준). */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Rope")
	float EndServerTime = 0.f;

	/** 이번 밧줄의 전체 길이(초). 게이지 비율 계산에 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Rope")
	float Length = 0.f;

	/** 멈춰 있는가. 멈춘 동안에는 불이 타지 않는다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Rope")
	bool bPaused = false;

	/**
	 * 멈춘 순간 남아 있던 시간(초). 멈춰 있는 동안 EndServerTime 은 의미가 없다 —
	 * 다시 붙일 때 이 값으로 끝나는 시각을 새로 계산한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Rope")
	float PausedRemaining = 0.f;
};

/**
 * 밧줄이 붙거나 꺼졌다. UI 는 이 이벤트만 구독하면 된다 — 매 프레임 폴링할 필요가 없다.
 * Seat 이 INDEX_NONE 이면 꺼진 것이고, 남은 시간은 GetSecondsRemaining 으로 매 프레임 읽는다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBGRopeChanged,
	int32, Seat, float, SecondsRemaining, float, TotalSeconds);

/**
 * 밧줄이 멈추거나 다시 탄다. 남은 시간은 그대로이므로 게이지를 지울 필요는 없다 —
 * 흐르지 않는다는 표시(색·아이콘)만 바꾸면 된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGRopePausedChanged, bool, bPaused);

UCLASS(Config = Game, ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGRopeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGRopeComponent();

	// 현재 월드의 GameState 에 붙은 컴포넌트. 클라에서도 찾을 수 있다(남은 시간 표시용).
	static UBGRopeComponent* Find(const UWorld* World);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── 서버 전용 ──

	// 턴이 열렸다. 좌석 상태에 맞는 길이로 불을 붙인다. BeginTurn 이 부른다.
	void BeginRope(int32 Seat);

	// 턴이 닫혔다(정상 종료·사망·판 종료). 불을 끈다.
	void StopRope();

	// ── 일시정지 (서버 전용) ──
	//
	// 이유(Reason)별로 따로 센다. 여러 곳이 동시에 잡을 수 있고, 마지막 하나가 놓아야 다시 탄다.
	// 그래서 연출이 끝나도 다른 이유가 남아 있으면 밧줄은 계속 멈춰 있다.

	/**
	 * 밧줄을 멈춘다. 같은 Reason 으로 여러 번 불러도 하나로 친다.
	 * 멈춘 동안에는 불이 타지 않으므로 턴이 넘어가지도 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Rope")
	void PauseRope(FName Reason);

	/** PauseRope 로 잡아 둔 것을 놓는다. 잡고 있던 이유가 모두 없어지면 다시 탄다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Rope")
	void ResumeRope(FName Reason);

	/** 잡고 있던 이유를 전부 놓는다. 판이 끝나거나 상태가 꼬였을 때만 쓸 것. */
	UFUNCTION(BlueprintCallable, Category = "BG|Rope")
	void ClearRopePauses();

	// ── 통지 (서버·클라 공통) ──
	//
	// 위젯이 밧줄보다 늦게 만들어지면 이벤트를 놓친다. 바인딩 직후 IsBurning() 을 한 번
	// 확인해 이미 타는 중이면 즉시 처리할 것(MatchEndStage 와 같은 관례다).
	UPROPERTY(BlueprintAssignable, Category = "BG|Rope")
	FOnBGRopeChanged OnRopeChanged;

	/** 멈춤 상태가 바뀌었다. 게이지를 흐리게 하거나 정지 아이콘을 띄우는 데 쓴다. */
	UPROPERTY(BlueprintAssignable, Category = "BG|Rope")
	FOnBGRopePausedChanged OnRopePausedChanged;

	// ── 조회 (서버·클라 공통) ──

	UFUNCTION(BlueprintPure, Category = "BG|Rope")
	bool IsBurning() const { return Rope.Seat != INDEX_NONE; }

	/** 멈춰 있는가. 밧줄이 안 붙은 상태에서도 이유가 잡혀 있으면 true 다. */
	UFUNCTION(BlueprintPure, Category = "BG|Rope")
	bool IsRopePaused() const { return Rope.bPaused; }

	/** 지금 밧줄이 걸린 좌석. 없으면 INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "BG|Rope")
	int32 GetRopeSeat() const { return Rope.Seat; }

	/** 이 좌석이 지금 밧줄을 태우고 있는가. 내 자리만 표시할 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Rope")
	bool IsSeatBurning(int32 Seat) const { return Seat != INDEX_NONE && Rope.Seat == Seat; }

	/** 남은 비율 0~1. 게이지에 그대로 넣는다. */
	UFUNCTION(BlueprintPure, Category = "BG|Rope")
	float GetRemainingRatio() const;

	// 남은 시간(초). 끝나는 시각만 복제하고 클라가 계산한다 — 매 프레임 복제하지 않기 위해서다.
	UFUNCTION(BlueprintPure, Category = "BG|Rope")
	float GetSecondsRemaining() const;

	/** 이번 밧줄의 전체 길이(초). 게이지 비율 계산에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Rope")
	float GetRopeLengthSeconds() const { return Rope.Length; }

	/** 자리를 비운 채 이만큼 넘기면 복귀를 포기한다. UI 안내에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Rope")
	int32 GetMaxStrikes() const { return MaxStrikes; }

protected:
	/** 접속 중인 사람의 턴 길이(초). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "BG|Rope")
	float TurnSeconds = 20.f;

	// 자리를 비운 사람의 턴 길이(초). 0 으로 두면 턴이 같은 프레임에 연쇄로 넘어가고,
	// 돌아오는 중이던 사람이 자기 턴을 통째로 잃는다.
	UPROPERTY(EditDefaultsOnly, Config, Category = "BG|Rope")
	float AbsentTurnSeconds = 3.f;

	/** 자리를 비운 채 밧줄이 이만큼 타면 그 좌석을 죽인다. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "BG|Rope")
	int32 MaxStrikes = 2;

	/**
	 * 이만큼 멈춰 있으면 잡고 있던 이유를 전부 놓는다. 0 이하면 감시하지 않는다.
	 * 놓아 주는 쪽이 빠지면 판이 영영 멈추므로 밧줄 자체에도 감시자를 둔다.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "BG|Rope")
	float PauseTimeoutSeconds = 15.f;

	// 불이 끝까지 탔다.
	void HandleRopeBurnedOut();

	// 잡고 있는 이유가 하나라도 있으면 멈추고, 하나도 없으면 다시 태운다.
	void ApplyPauseState();

	// 너무 오래 멈춰 있다. 잡고 있던 이유를 전부 놓아 판을 다시 굴린다.
	void HandlePauseTimeout();

	ABGGameState* GetGameState() const;

	UFUNCTION()
	void OnRep_Rope();

	//	리슨서버 호스트는 자기 OnRep 을 받지 못한다 — 서버는 값을 바꾼 자리에서 직접 부른다.
	void BroadcastRope();

	UPROPERTY(ReplicatedUsing = OnRep_Rope)
	FBGRopeState Rope;

	//	서버 전용. 실제 만료를 재는 타이머.
	FTimerHandle RopeTimer;

	//	마지막으로 알린 멈춤 상태. 같은 값을 두 번 알리지 않는다.
	bool BroadcastedPaused = false;

	//	서버 전용. 지금 밧줄을 잡고 있는 이유들. 비면 다시 탄다.
	TArray<FName> PauseHolders;

	//	서버 전용. 놓아 주는 쪽이 빠졌을 때를 대비한 감시 타이머.
	FTimerHandle PauseWatchdog;
};

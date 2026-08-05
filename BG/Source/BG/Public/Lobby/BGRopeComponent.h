// BANG! 밧줄 — 턴 제한 시간(하스스톤의 그 밧줄). 다 타면 턴이 자동으로 넘어간다.
// 자리를 비운 사람의 턴에서 판이 멈추는 것을 막는 워치독이다(Prototype-Workflow §7).
// 연속 MaxStrikes 번 타면 그 좌석을 죽여 기존 사망 경로로 승패를 처리한다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BGRopeComponent.generated.h"

class ABGGameState;

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
};

/**
 * 밧줄이 붙거나 꺼졌다. UI 는 이 이벤트만 구독하면 된다 — 매 프레임 폴링할 필요가 없다.
 * Seat 이 INDEX_NONE 이면 꺼진 것이고, 남은 시간은 GetSecondsRemaining 으로 매 프레임 읽는다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBGRopeChanged,
	int32, Seat, float, SecondsRemaining, float, TotalSeconds);

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

	// ── 통지 (서버·클라 공통) ──
	//
	// 위젯이 밧줄보다 늦게 만들어지면 이벤트를 놓친다. 바인딩 직후 IsBurning() 을 한 번
	// 확인해 이미 타는 중이면 즉시 처리할 것(MatchEndStage 와 같은 관례다).
	UPROPERTY(BlueprintAssignable, Category = "BG|Rope")
	FOnBGRopeChanged OnRopeChanged;

	// ── 조회 (서버·클라 공통) ──

	UFUNCTION(BlueprintPure, Category = "BG|Rope")
	bool IsBurning() const { return Rope.Seat != INDEX_NONE; }

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

	// 불이 끝까지 탔다.
	void HandleRopeBurnedOut();

	ABGGameState* GetGameState() const;

	UFUNCTION()
	void OnRep_Rope();

	//	리슨서버 호스트는 자기 OnRep 을 받지 못한다 — 서버는 값을 바꾼 자리에서 직접 부른다.
	void BroadcastRope();

	UPROPERTY(ReplicatedUsing = OnRep_Rope)
	FBGRopeState Rope;

	//	서버 전용. 실제 만료를 재는 타이머.
	FTimerHandle RopeTimer;
};

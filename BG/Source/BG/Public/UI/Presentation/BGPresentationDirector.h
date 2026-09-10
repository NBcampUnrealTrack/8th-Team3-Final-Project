// 카드 연출의 순서를 정하는 곳. PlayerController 에 붙는 클라 전용 컴포넌트.
//
// 하는 일은 **언제**뿐이다 — 재생은 여전히 위젯·BP 몫이다.
//
// ── 완료 판정: 트랙 ────────────────────────────────────────────────────────────
//  연출은 책임이 다른 갈래(트랙)로 이뤄진다 — 시네마틱 / 월드FX / 몽타주 / UI.
//  각 트랙은 **끝내는 주체가 하나로 정해져 있고**(EBGPresentationTrack 주석),
//  자기가 도는 동안 Hold 로 표를 쥔다. 표가 전부 놓이면 연출이 끝난다.
//
//  디렉터는 트랙의 내용을 모른다. 몇 개가 도는지만 센다 — 참여자가 늘어도
//  디렉터를 고칠 일이 없고, 멈추면 어느 트랙이 안 끝났는지 로그에 이름으로 찍힌다.
//
//  ⚠️ 표를 아무도 잡지 않은 연출은 곧바로 끝난다. 기다릴 것이 없다는 뜻이라 맞는 동작이다 —
//     최소 길이가 필요하면 그 길이를 아는 쪽이 UI 트랙을 쥐면 된다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/Presentation/BGPresentationTypes.h"
#include "BGPresentationDirector.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGPresentationStarted,
	const FBGPresentationRequest&, Request);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGPresentationFinished,
	const FBGPresentationRequest&, Request);

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGPresentationDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGPresentationDirector();

	/**
	 * 이 클라이언트의 연출 큐. 연출은 화면마다 따로 도므로 로컬 PlayerController 것을 찾는다.
	 * 데디 서버·PC 생성 전이면 nullptr — 흔한 일이므로 부르는 쪽이 조용히 넘어가면 된다.
	 *
	 * ⚠️ 분할 화면은 고려하지 않는다(첫 PlayerController 를 쓴다). 이 게임은 1인 1창이다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Presentation", meta = (WorldContext = "WorldContextObject"))
	static UBGPresentationDirector* Get(const UObject* WorldContextObject);

	/** 연출을 줄에 세운다. 비어 있으면 곧바로 시작한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Presentation")
	void Enqueue(const FBGPresentationRequest& Request);

	/** 카드 사용 연출을 줄에 세운다 — Enqueue 의 흔한 경우를 감싼 것. */
	UFUNCTION(BlueprintCallable, Category = "BG|Presentation")
	void EnqueueCardPlay(const FBGCardPlayContext& Context);

	/**
	 * "이 갈래가 아직 안 끝났다" 를 등록한다. 트랙마다 끝내는 주체가 하나로 정해져 있다 —
	 * EBGPresentationTrack 주석 참고.
	 * ⚠️ 연출이 시작되기 전에 잡아도 된다 — 그 표는 다음 연출로 넘어간다.
	 *    몽타주는 카드가 중앙에 닿기 전에 시작하므로 이 경우가 오히려 정상이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Presentation")
	FBGPresentationHandle Hold(EBGPresentationTrack Track);

	/** 잡아 둔 표를 놓는다. 전부 놓이면 연출이 끝난다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Presentation")
	void Release(FBGPresentationHandle Handle);

	UFUNCTION(BlueprintPure, Category = "BG|Presentation")
	bool IsPresenting() const { return bPresenting; }


	UFUNCTION(BlueprintPure, Category = "BG|Presentation")
	const FBGPresentationRequest& GetActiveRequest() const { return Active; }

	/** 연출이 시작됐다. HUD·카드·좌석 보드가 각자 자기 몫을 재생한다. */
	UPROPERTY(BlueprintAssignable, Category = "BG|Presentation")
	FOnBGPresentationStarted OnPresentationStarted;

	/** 표가 전부 놓였거나 상한에 걸렸다. 켜 둔 연출을 내린다. */
	UPROPERTY(BlueprintAssignable, Category = "BG|Presentation")
	FOnBGPresentationFinished OnPresentationFinished;

	/**
	 * 카드 사용 연출의 바닥(초). EnqueueCardPlay 가 요청에 실어 준다.
	 * ⚠️ 카드가 중앙까지 날아가는 시간(PresentDuration)보다 짧으면 의미가 없다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Presentation", meta = (ClampMin = "0.0"))
	float CardPlayMinDuration = 0.4f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 줄의 맨 앞을 시작한다. 이미 진행 중이거나 줄이 비었으면 아무 일도 없다. */
	void StartNext();

	/** 진행 중인 연출을 끝내고 다음으로 넘어간다. */
	void FinishActive(bool bTimedOut);

	void HandleTimeout();

	/** 바닥 시간이 지났다 — 붙잡아 두던 표를 놓는다. */
	void HandleMinDurationElapsed();

	UPROPERTY()
	TArray<FBGPresentationRequest> Queue;

	UPROPERTY()
	FBGPresentationRequest Active;

	bool bPresenting = false;

	/** StartNext 재진입 가드. FinishActive 가 되부르는 경로를 막는다. */
	bool bAdvancing = false;

	/** 시작 통지를 뿌리는 중인가. 그동안의 종료는 통지가 끝난 뒤로 미룬다. */
	bool bBroadcastingStart = false;
	bool bFinishRequestedDuringStart = false;

	/** 아직 놓이지 않은 표 → 그 표의 트랙. 비면 연출이 끝난다. */
	TMap<int32, EBGPresentationTrack> ActiveHolds;

	/** 연출 시작 전에 잡힌 표들. StartNext 가 ActiveHolds 로 물려받는다. */
	TMap<int32, EBGPresentationTrack> PendingHolds;

	int32 NextHandleId = 0;

	FTimerHandle TimeoutTimer;

	/** 바닥 시간 동안 디렉터가 쥐고 있는 표. 참여자가 없어도 연출이 즉시 끝나지 않게 한다. */
	FBGPresentationHandle MinDurationHold;
	FTimerHandle MinDurationTimer;
};

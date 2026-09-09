// 카드 연출의 순서를 정하는 곳. PlayerController 에 붙는 클라 전용 컴포넌트.
//
// 하는 일은 **언제**뿐이다 — 재생은 여전히 위젯·BP 몫이다.
// 연출 상태(무엇이 진행 중인가)를 여기서 들기 때문에, 판마다 죽는 카드 위젯이
// 더 이상 그것을 대신 들지 않아도 된다.
//
// 완료는 핸드셰이크로 판단한다: 시간이 걸리는 구독자가 Hold 로 표를 잡고 끝나면
// Release 한다. 전부 놓여야 다음 연출이 시작된다 — 구독자가 몇인지 디렉터는 모른다.

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

	/** 연출을 줄에 세운다. 비어 있으면 곧바로 시작한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Presentation")
	void Enqueue(const FBGPresentationRequest& Request);

	/** 카드 사용 연출을 줄에 세운다 — Enqueue 의 흔한 경우를 감싼 것. */
	UFUNCTION(BlueprintCallable, Category = "BG|Presentation")
	void EnqueueCardPlay(const FBGCardPlayContext& Context);

	/**
	 * 월드 연출(시네마틱 단계 종료 · GameplayCue 종료)이 끝났다.
	 * 디렉터가 시작 시 잡아 둔 기본 표를 놓는다. 여러 번 불러도 안전하다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Presentation")
	void NotifyWorldFXFinished();

	/**
	 * "나도 끝나야 한다" 를 등록한다. 진행 중인 연출이 없으면 무효 표를 준다.
	 * 몽타주 종료 · UMG 애니메이션 Finished 처럼 시간이 걸리는 구독자가 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Presentation")
	FBGPresentationHandle Hold();

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

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 줄의 맨 앞을 시작한다. 이미 진행 중이거나 줄이 비었으면 아무 일도 없다. */
	void StartNext();

	/** 진행 중인 연출을 끝내고 다음으로 넘어간다. */
	void FinishActive(bool bTimedOut);

	void HandleTimeout();

	UPROPERTY()
	TArray<FBGPresentationRequest> Queue;

	UPROPERTY()
	FBGPresentationRequest Active;

	bool bPresenting = false;

	/** 아직 놓이지 않은 표들. 비면 연출이 끝난다. */
	TSet<int32> ActiveHolds;

	/** 디렉터가 시작 시 스스로 잡는 표. NotifyWorldFXFinished 가 이것을 놓는다. */
	FBGPresentationHandle WorldFXHold;

	int32 NextHandleId = 0;

	FTimerHandle TimeoutTimer;
};

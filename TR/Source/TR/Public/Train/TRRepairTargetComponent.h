// TRRepairTargetComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TRRepairTargetComponent.generated.h"

/**
 * 이 액터의 수리 사이클을 소유한다. 장갑·포탑·기차에 붙는다.
 *
 * 사이클을 사람이 아니라 대상이 갖는 이유 — 같은 대상을 보는 모든 플레이어가 같은
 * 게이지를 봐야 한다. 사람마다 타이머를 돌리면 시작 시각이 달라 게이지가 어긋난다.
 *
 * 붙은 인원이 늘면 사이클이 짧아지고, 전원이 손을 떼면 리셋된다.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TR_API UTRRepairTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UTRRepairTargetComponent();
	
	UFUNCTION(BlueprintPure, Category="Repair")
	float GetCycleProgress01() const;
	
	UFUNCTION(BlueprintPure, Category="Repair")
	bool IsCycleRunning() const { return CycleDuration > 0.f; }

	void RefreshCycle(int32 WorkerCount);
	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	float ResolveCycleDuration(int32 WorkerCount) const;
	
	void HandleCycleComplete();

	// 소유자를 더 고칠 수 있는지. 장갑·포탑과 기차가 공통 부모가 없어 두 갈래로 본다.
	bool IsOwnerRepairable() const;
	
	UPROPERTY(EditDefaultsOnly, Category="Repair")
	float BaseCycleDuration = 5.f;
	
	// 인원이 늘수록 사이클이 짧아지는 배수. 인덱스 = 인원-1.
	UPROPERTY(EditDefaultsOnly, Category="Repair")
	TArray<float> ConcurrentCycleSpeedup = { 1.f, 1.5f, 2.f };

	// 사이클 시작 서버 시각. 인원이 바뀌면 진행률을 보존하려고 과거로 역산해 덮는다.
	UPROPERTY(Replicated)
	float CycleStartServerTime = 0.f;

	// 인원 배수가 적용된 실제 사이클 시간. 0 이면 사이클이 돌지 않는 상태다.
	UPROPERTY(Replicated)
	float CycleDuration = 0.f;

	FTimerHandle CycleTimerHandle;

	// 완료 처리 중 표식.
	bool bCompletingCycle = false;
};

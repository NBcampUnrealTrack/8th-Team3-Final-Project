// Interaction/TRActionComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TRTypes.h"
#include "TRActionComponent.generated.h"

class AActor;

UCLASS(ClassGroup=(TrainDefense), meta=(BlueprintSpawnableComponent))
class TR_API UTRActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTRActionComponent();

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="Action")
	void ServerStartAction(ETRActionType ActionType, AActor* Target);
	bool ServerStartAction_Validate(ETRActionType ActionType, AActor* Target);
	void ServerStartAction_Implementation(ETRActionType ActionType, AActor* Target);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Action")
	void ServerCancelAction();
	void ServerCancelAction_Implementation();
	
	UFUNCTION(BlueprintPure, Category="Action")
	bool IsActionInProgress() const { return CurrentActionType != ETRActionType::None; }

	UFUNCTION(BlueprintPure, Category="Action")
	ETRActionType GetCurrentActionType() const { return CurrentActionType; }

	// 지금 작업 중인 대상. 진행 중이 아니면 nullptr.
	UFUNCTION(BlueprintPure, Category="Action")
	AActor* GetCurrentActionTarget() const { return CurrentTarget.Get(); }

	// 재설치는 열차 전체에서 한 번에 하나만 돈다. 진행 중인 컴포넌트를 찾는다. 없으면 nullptr.
	static UTRActionComponent* FindBuildingComponent(UWorld* World);

	UFUNCTION(BlueprintPure, Category="Action")
	float GetActionProgress01() const;

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 타이머 완료 시 서버에서 호출 (수리 반영, 자원 차감 등 실제 효과 적용)
	void CompleteAction();

	bool ValidateActionTarget(ETRActionType ActionType, AActor* Target) const;

	// 진행 상태를 바꾸는 유일한 통로. GameState 의 재설치 표식도 여기서만 갱신해 둘이 어긋나지 않게 한다.
	void SetCurrentAction(ETRActionType NewType, AActor* NewTarget);

	// 복제된 행동 상태를 오너 캐릭터의 지속 이펙트에 반영한다.
	void RefreshOwnerActionEffect();

	// 같은 대상에서 같은 작업을 하는 인원. 자기 자신을 포함한다.
	int32 CountConcurrentWorkers(ETRActionType ActionType, const AActor* Target) const;

	// 인원수에 맞는 유효 사이클 시간. 수리가 아니거나 배수 배열이 비면 CycleDuration 그대로.
	float ResolveCycleDuration(ETRActionType ActionType, const FTRActionDef& Def, int32 WorkerCount) const;

	// 지금까지의 진행률을 보존하면서 유효 사이클 시간을 다시 적용한다. 인원이 바뀌는 순간에만 부른다.
	void RecomputeCycleTiming();

	// 다른 컴포넌트들의 사이클 시간을 다시 계산. 내 상태를 바꾼 뒤에 부를 것(인원 집계에 내가 포함되도록).
	void RefreshConcurrentWorkers(ETRActionType ActionType, AActor* Target);

	UFUNCTION()
	void OnRep_CurrentActionType();

	bool RequiresMonsterResource(ETRActionType ActionType) const { return ActionType == ETRActionType::Build; }
	
	// RPC 순간값 대신 행동 상태를 복제한다.
	UPROPERTY(ReplicatedUsing = OnRep_CurrentActionType)
	ETRActionType CurrentActionType = ETRActionType::None;

	UPROPERTY(Replicated)
	float ActionStartServerTime = 0.f;

	// 인원 배수가 적용된 실제 사이클 시간. 진행 바가 이 값으로 진행률을 잰다.
	UPROPERTY(Replicated)
	float EffectiveCycleDuration = 0.f;

	UPROPERTY(Replicated)
	TWeakObjectPtr<AActor> CurrentTarget;

	FTimerHandle ActionTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category="Action")
	TMap<ETRActionType, FTRActionDef> ActionDefs;

	// 여러 명이 같은 대상을 함께 수리할 때의 속도 배수. 인덱스 = 인원-1.
	// 수리 전용이다. 재장전·재보급은 인원과 무관하게 기준 시간을 쓴다.
	UPROPERTY(EditDefaultsOnly, Category="Action")
	TArray<float> RepairConcurrentSpeedMultipliers = { 1.f, 1.5f, 2.f };
};

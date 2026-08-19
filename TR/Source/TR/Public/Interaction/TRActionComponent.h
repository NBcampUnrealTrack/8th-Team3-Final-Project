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
		
	UFUNCTION(BlueprintPure, Category="Action")
	float GetActionProgress01() const;
	
	// 재설치는 열차 전체에서 한 번에 하나만 돈다. 진행 중인 컴포넌트를 찾는다. 없으면 nullptr.
	static UTRActionComponent* FindBuildingComponent(UWorld* World);

	static void GatherRepairWorkers(const UWorld* World, const AActor* Target, TArray<UTRActionComponent*>& OutWorkers);
	
	bool CommitRepairContribution();

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

	void NotifyRepairTarget(ETRActionType ActionType, AActor* Target);

	UFUNCTION()
	void OnRep_CurrentActionType();

	bool RequiresMonsterResource(ETRActionType ActionType) const { return ActionType == ETRActionType::Build; }
	
	// RPC 순간값 대신 행동 상태를 복제한다.
	UPROPERTY(ReplicatedUsing = OnRep_CurrentActionType)
	ETRActionType CurrentActionType = ETRActionType::None;

	UPROPERTY(Replicated)
	float ActionStartServerTime = 0.f;

	UPROPERTY(Replicated)
	TWeakObjectPtr<AActor> CurrentTarget;

	FTimerHandle ActionTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category="Action")
	TMap<ETRActionType, FTRActionDef> ActionDefs;
};

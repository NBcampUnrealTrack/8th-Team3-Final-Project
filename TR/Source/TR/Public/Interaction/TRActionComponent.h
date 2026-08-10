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

	UFUNCTION(BlueprintPure, Category="Action")
	float GetActionProgress01() const;

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 타이머 완료 시 서버에서 호출 (수리 반영, 자원 차감 등 실제 효과 적용)
	void CompleteAction();

	bool ValidateActionTarget(ETRActionType ActionType, AActor* Target) const;

	bool RequiresMonsterResource(ETRActionType ActionType) const { return ActionType == ETRActionType::Build; }
	
	UPROPERTY(Replicated)
	ETRActionType CurrentActionType = ETRActionType::None;

	UPROPERTY(Replicated)
	float ActionStartServerTime = 0.f;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	FTimerHandle ActionTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category="Action")
	TMap<ETRActionType, FTRActionDef> ActionDefs;
};

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

	// 키를 누르는 순간 호출
	UFUNCTION(BlueprintCallable, Category="Action")
	void RequestStartAction(ETRActionType ActionType, AActor* Target);

	// 키를 떼거나, 이동 등으로 액션이 끊길 때 호출
	UFUNCTION(BlueprintCallable, Category="Action")
	void RequestCancelAction();

	UFUNCTION(BlueprintPure, Category="Action")
	bool IsActionInProgress() const { return CurrentActionType != ETRActionType::None; }

	// TODO: UI 붙을 때 구현. 지금은 0 반환.
	UFUNCTION(BlueprintPure, Category="Action")
	float GetActionProgress01() const;

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartAction(ETRActionType ActionType, AActor* Target);
	bool ServerStartAction_Validate(ETRActionType ActionType, AActor* Target);
	void ServerStartAction_Implementation(ETRActionType ActionType, AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerCancelAction();
	void ServerCancelAction_Implementation();

	// 타이머 완료 시 서버에서 호출 (수리 반영, 자원 차감 등 실제 효과 적용)
	void CompleteAction();

	// TODO: 대상 클래스(터렛 등) 확정되면 구현.
	//  - 대상이 해당 ActionType을 받을 수 있는 상태인지 (예: 풀내구도 터렛에 Repair 불가)
	//  - 대상과의 거리/상호작용 반경
	bool ValidateActionTarget(ETRActionType ActionType, AActor* Target) const;

	UPROPERTY(Replicated)
	ETRActionType CurrentActionType = ETRActionType::None;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	FTimerHandle ActionTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category="Action")
	TMap<ETRActionType, FTRActionDef> ActionDefs;

	bool RequiresMonsterResource(ETRActionType ActionType) const { return ActionType == ETRActionType::Build; }
};
// TRInteractionComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TRInteractionComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TR_API UTRInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UTRInteractionComponent();

	// 현재 라인 트레이스가 잡고 있는 대상. 없으면 nullptr.
	UFUNCTION(BlueprintPure, Category="Interaction")
	AActor* GetCurrentTarget() const { return LastTraceResult.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void UpdateTrace();

	void UpdateInteractionWidget();

	// 이건 매번 새로 계산한 결과를 다음 조회 전까지만 들고 있는 캐시
	TWeakObjectPtr<AActor> LastTraceResult;

	UPROPERTY(EditDefaultsOnly, Category="Interaction")
	float TraceDistance = 200.f;
};

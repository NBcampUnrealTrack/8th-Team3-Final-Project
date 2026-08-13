// TRChaseSteeringProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRChaseSteeringProcessor.generated.h"

/**
 * 목표를 향한 조종력 생성.
 *
 * FTRTargetFragment의 목표 위치를 보고 원하는 속도를 계산해 FMassForceFragment에 누적한다.
 * 실제 속도 적분과 위치 갱신은 엔진의 UMassApplyForceProcessor → UMassApplyMovementProcessor가 처리한다.
 *
 * Avoidance 그룹에서 돌아 ApplyForces 그룹보다 먼저 실행된다.
 */
UCLASS()
class TR_API UTRChaseSteeringProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRChaseSteeringProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

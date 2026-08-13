// TRChaseSeparationProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRChaseSeparationProcessor.generated.h"

/**
 * 이웃끼리 서로 밀어내는 힘을 만든다.
 *
 * 목표가 한 점이면 전원이 그 점으로 수렴해 겹쳐버린다. 개체 수가 늘수록 심해져서
 * 무리가 아니라 한 덩어리로 보인다. 이 프로세서가 그걸 푼다.
 *
 * 이웃 탐색은 MassNavigation의 해시 그리드를 재사용한다. Config에
 * "Navigation Obstacle" 트레이트를 추가하면 엔진이 그리드를 자동으로 유지해준다.
 * (같은 그리드를 나중에 포탑의 적 탐색에도 쓸 수 있다.)
 *
 * Steering과 같은 Avoidance 그룹에서 돌며, 둘 다 같은 FMassForceFragment에 힘을 누적한다.
 */
UCLASS()
class TR_API UTRChaseSeparationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRChaseSeparationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

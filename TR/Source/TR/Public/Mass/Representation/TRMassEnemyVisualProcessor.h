// TRMassEnemyVisualProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRMassEnemyVisualProcessor.generated.h"

/**
 * 전투 수치를 표현용 손상 상태(FTREnemyVisualFragment)로 옮긴다.
 *
 * **서버 전용이다.** 체력은 서버에만 있으므로 여기서만 계산할 수 있고, 클라이언트의
 * 같은 프래그먼트는 ATRMassNetRelay 가 스냅샷으로 채운다. 두 경로가 같은 프래그먼트를
 * 쓰기 때문에 표현 쪽(액터·ISM)은 어느 쪽이 채웠는지 몰라도 된다.
 *
 * 피해 이벤트를 따로 받지 않고 손상도가 **올라간 것**으로 피격을 감지한다. 전투 쪽에
 * 이벤트를 추가하면 그 이벤트가 표현 계층까지 오는 경로를 하나 더 유지해야 하는데,
 * 이미 매 틱 들어오는 값으로 같은 판단을 할 수 있다.
 */
UCLASS()
class TR_API UTRMassEnemyVisualProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRMassEnemyVisualProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

// TRTargetSyncProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRTargetSyncProcessor.generated.h"

/**
 * 채널 → 목표 위치 동기화.
 *
 * UTRTargetDirectorSubsystem이 들고 있는 채널별 목표를 각 엔티티의 FTRTargetFragment에 복사한다.
 * 실제 조회는 프레임당 채널 수만큼(현재 4회)만 일어나고 엔티티 루프는 배열 인덱싱뿐이라,
 * 엔티티가 100마리든 10000마리든 서브시스템 조회 비용은 동일하다.
 *
 * Behavior 그룹에서 돌아 Steering(Avoidance 그룹)보다 먼저 실행된다.
 */
UCLASS()
class TR_API UTRTargetSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRTargetSyncProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

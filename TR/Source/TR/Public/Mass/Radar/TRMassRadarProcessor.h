// TRMassRadarProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRMassRadarProcessor.generated.h"

/**
 * 살아 있는 적의 위치를 UTRRadarSubsystem 으로 옮긴다.
 *
 * 표현 액터를 보지 않는다는 점이 이 프로세서의 전부다. 액터는 LOD 안쪽에만
 * 있지만 트랜스폼 프래그먼트는 모든 엔티티에 있으므로, 화면 밖 수 km 떨어진
 * 적도 레이더에는 정확히 찍힌다.
 *
 * UpdateWorldFromMass 그룹에서 도는 이유는 이동·지면 추종이 모두 끝난 뒤의
 * 최종 위치를 읽기 위해서다. 그보다 앞에서 읽으면 레이더만 한 프레임 뒤처진다.
 */
UCLASS()
class TR_API UTRMassRadarProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRMassRadarProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

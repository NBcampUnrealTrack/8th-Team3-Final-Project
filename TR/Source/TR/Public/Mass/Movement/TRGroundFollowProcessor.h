// TRGroundFollowProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRGroundFollowProcessor.generated.h"

/**
 * 엔티티를 지면 높이에 붙여둔다.
 *
 * Mass 엔티티에는 물리도 캡슐도 없다. 이동 프로세서는 수평 속도만 적분하므로,
 * 아무것도 하지 않으면 스폰 당시 높이를 영원히 유지한다. 그러면 지형이 오르내릴 때
 * 공중에 뜨거나 땅에 묻히고, 액터 표현과 인스턴스 표현의 높이가 어긋나 보인다.
 *
 * 비용 때문에 매 프레임 트레이스하지 않는다. 개체마다 주기를 두고 트레이스하되
 * 시작 시각을 흩어 한 프레임에 몰리지 않게 하며, 그 사이는 보간으로 메운다.
 *
 * 이동이 끝난 뒤에 실행되어야 최종 위치를 보정할 수 있다.
 */
UCLASS()
class TR_API UTRGroundFollowProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRGroundFollowProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

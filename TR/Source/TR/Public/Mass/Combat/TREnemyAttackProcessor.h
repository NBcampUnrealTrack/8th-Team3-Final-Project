// TREnemyAttackProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TREnemyAttackProcessor.generated.h"

/**
 * 목표에 도달한 적이 종류에 맞는 행동을 하게 한다.
 *
 *   근접형   접촉 → 준비 시간 → 폭발(피해) → 소멸
 *   사출형   사출 거리 도달 → 상체 사출(같은 엔티티가 빠른 투사체가 된다) → 접촉 시 폭발
 *   사격형   유지 거리 도달 → 간격마다 사격 → 탄 소진 시 자체 폭발(피해 없음)
 *
 * **도달 판정은 하지 않는다.** UTRChaseSteeringProcessor가 히스테리시스까지 적용해
 * 붙여준 FTRArrivedTag를 필터로 쓴다. 같은 판정을 두 곳에서 하면 경계에서 어긋난다.
 *
 * 피해는 액터에 직접 넣지 않고 UTRMassCombatSubsystem 큐에 넣는다. 프로세서는
 * 워커 스레드에서 돌 수 있어서 액터를 만지면 안 되기 때문이다.
 */
UCLASS()
class TR_API UTREnemyAttackProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTREnemyAttackProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	/** 도달했는데 아직 행동을 시작하지 않은 엔티티. 종류에 따라 분기한다. */
	FMassEntityQuery ArrivalQuery;

	/**
	 * 폭발이 예약된 엔티티. 도달 태그와 무관하게 잡는다 —
	 * 예약 뒤에 기차가 멀어져 도달이 풀려도 폭발은 진행되어야 한다.
	 */
	FMassEntityQuery DetonationQuery;
};

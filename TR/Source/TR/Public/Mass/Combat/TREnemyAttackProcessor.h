// TREnemyAttackProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TREnemyAttackProcessor.generated.h"

/**
 * 예약된 폭발을 시각이 되면 실행한다.
 *
 * **무엇을 언제 할지는 정하지 않는다.** 그 판단은 전부 StateTree가 한다:
 *
 *   근접형   FTRDetonateTask 가 준비 시각을 적고 FTRDetonatingTag 를 붙인다
 *   사출형   FTRLaunchTask   가 상체를 사출하고, 그 뒤에는 근접형으로 행동한다
 *   사격형   FTRFireTask     가 간격마다 쏘고, 탄이 떨어지면 FTRDeathTask 로 넘어간다
 *
 * 예전에는 이 프로세서가 위 세 가지를 **StateTree와 나란히 중복 수행**했다. 두 경로가
 * 서로를 상태 비교로 피해 다녔는데, 가드가 곳곳에서 모양이 달랐고 사격형에는 아예
 * 없어서 어느 쪽이 실제로 행동을 시작했는지 추적하기 어려웠다. 지금은 판단이
 * StateTree 한 곳에만 있고, 여기는 그 결과를 실행하는 역할만 한다.
 *
 * 폭발만 프로세서에 남긴 이유: 준비 시각이 지났는지는 매 틱 전 엔티티를 훑어야 하는
 * 일이고, 그건 태그로 필터링되는 쿼리가 트리 순회보다 훨씬 싸다.
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
	/**
	 * 폭발이 예약된 엔티티. 도달 태그와 무관하게 잡는다 —
	 * 예약 뒤에 기차가 멀어져 도달이 풀려도 폭발은 진행되어야 한다.
	 */
	FMassEntityQuery DetonationQuery;
};

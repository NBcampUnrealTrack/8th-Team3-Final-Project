// TRMassAnimSelectProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRMassAnimSelectProcessor.generated.h"

/**
 * 행동 상태를 인스턴스 스킨드 메시의 시퀀스 인덱스로 옮긴다.
 *
 * 원거리 표현(ISM)에는 AnimBP가 없다. 엔티티마다 FMassRepresentationAnimationFragment에
 * 들어 있는 SequenceIndex 하나가 곧 포즈이고, 그 값을 UMassConsumeInstancedSkinnedMeshAnimationProcessor가
 * 읽어 렌더 쪽으로 넘긴다. 그런데 **엔진에는 그 값을 써주는 프로세서가 없다** — 소비 쪽만
 * 있고 생산 쪽은 게임이 만들어야 한다. 이게 없으면 전원이 인덱스 0에 고정된다.
 *
 * 매핑 규칙은 UE::TR::Enemy::SelectAnim에 있고, 인덱스 자체는 FTREnemyAnimParams로
 * 애셋에서 지정한다(종류마다 애님 세트가 달라 코드에 박을 수 없다).
 *
 * 알아둘 것:
 *  - 렌더 쪽은 **시퀀스 인덱스가 바뀔 때만** 트랙을 다시 설정한다. 그래서 매 프레임 같은
 *    값을 써넣어도 재생이 리셋되지 않는 대신, 인덱스를 그대로 둔 채 PlayRate만 바꾸는
 *    조작은 반영되지 않는다.
 *  - 재생 종료 콜백도 없다. 단발 동작은 FTREnemyStateFragment::StateEnteredTime과
 *    파라미터의 Duration으로 직접 시간을 재서 로코모션으로 되돌린다.
 */
UCLASS()
class TR_API UTRMassAnimSelectProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRMassAnimSelectProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

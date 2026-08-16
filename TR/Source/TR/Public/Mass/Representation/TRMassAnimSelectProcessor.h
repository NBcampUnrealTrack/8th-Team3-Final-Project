// TRMassAnimSelectProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRMassAnimSelectProcessor.generated.h"

/**
 * 행동 상태를 인스턴스 스킨드 메시의 시퀀스 인덱스로 옮긴다.
 *
 * ISM에는 AnimBP가 없다. FMassRepresentationAnimationFragment의 SequenceIndex 하나가 곧
 * 포즈이고, **엔진에는 그 값을 써주는 프로세서가 없다** — 소비 쪽만 있고 생산 쪽은 게임 몫이다.
 * 이게 없으면 전원이 인덱스 0에 고정된다. 판단은 UE::TR::Enemy::ResolveAnim이 한다.
 *
 * 알아둘 것:
 *  - 렌더는 **인덱스가 바뀔 때만** 트랙을 다시 설정한다. 인덱스를 둔 채 PlayRate만 바꾸는
 *    조작은 반영되지 않는다.
 *  - 재생 종료 콜백이 없다. 단발은 StateEnteredTime과 Duration으로 시간을 직접 재서 되돌린다.
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

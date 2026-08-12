// TRStateTreeTasks.cpp

#include "Mass/StateTree/TRStateTreeTasks.h"

#include "Engine/World.h"
#include "MassCommandBuffer.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Mass/Debug/TRMassDebug.h"

namespace
{
	float GetWorldTime(const FStateTreeExecutionContext& Context)
	{
		return Context.GetWorld() ? Context.GetWorld()->GetTimeSeconds() : 0.f;
	}
}

//----------------------------------------------------------------------//
//  FTRMoveToTargetTask
//----------------------------------------------------------------------//

bool FTRMoveToTargetTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyStateHandle);
	return true;
}

void FTRMoveToTargetTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	// 프로세서 쿼리의 AddRequirement와 같은 역할이다. 선언하지 않으면
	// StateTree가 이 프래그먼트에 접근할 수 있다는 걸 Mass가 알지 못한다.
	Builder.AddReadWrite<FTREnemyStateFragment>();
}

EStateTreeRunStatus FTRMoveToTargetTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	EnterActionState(Context.GetExternalData(EnemyStateHandle), ETREnemyActionState::None, GetWorldTime(Context));

	// 이동 자체는 프로세서가 한다. 이 상태는 전이 조건이 참이 될 때까지 유지되기만 하면 된다.
	return EStateTreeRunStatus::Running;
}

//----------------------------------------------------------------------//
//  FTRDetonateTask
//----------------------------------------------------------------------//

bool FTRDetonateTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyStateHandle);
	Linker.LinkExternalData(CombatHandle);
	return true;
}

void FTRDetonateTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FTREnemyStateFragment>();
	Builder.AddReadWrite<FTREnemyCombatFragment>();
}

EStateTreeRunStatus FTRDetonateTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	const float WorldTime = GetWorldTime(Context);
	EnterActionState(Context.GetExternalData(EnemyStateHandle), ETREnemyActionState::Attack, WorldTime);

	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 폭발 시각을 적어두고 예약 태그만 붙인다. 실행은 UTREnemyAttackProcessor가 한다.
	// 트리를 깨우는 지연 신호도 필요 없어졌다 — 프로세서는 매 틱 시각만 비교하면 되고,
	// 신호에 의존하면 트리 구성이 어긋났을 때 조용히 안 터지는 상태가 된다.
	FTREnemyCombatFragment& Combatant = Context.GetExternalData(CombatHandle);
	Combatant.NextActionTime = WorldTime + InstanceData.WindupDuration;

	MassContext.GetMassEntityExecutionContext().Defer().AddTag<FTRDetonatingTag>(MassContext.GetEntity());

	// 폭발과 함께 엔티티가 사라지므로 이 상태가 스스로 끝날 일은 없다.
	return EStateTreeRunStatus::Running;
}

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

bool FTRLaunchTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyStateHandle);
	Linker.LinkExternalData(CombatHandle);
	Linker.LinkExternalData(ApproachHandle);
	Linker.LinkExternalData(TargetHandle);
	Linker.LinkExternalData(CombatParamsHandle);
	return true;
}

void FTRLaunchTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FTREnemyStateFragment>();
	Builder.AddReadWrite<FTREnemyCombatFragment>();
	Builder.AddReadWrite<FTRApproachFragment>();
	Builder.AddReadWrite<FTRTargetFragment>();
	Builder.AddReadOnly<FTREnemyCombatParams>();
}

EStateTreeRunStatus FTRLaunchTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	const FTREnemyCombatParams& Params = Context.GetExternalData(CombatParamsHandle);
	FTREnemyCombatFragment& Combat = Context.GetExternalData(CombatHandle);
	FTRApproachFragment& Approach = Context.GetExternalData(ApproachHandle);

	UE::TR::Enemy::ApplyLaunched(Params, Combat, Approach);
	EnterActionState(Context.GetExternalData(EnemyStateHandle), ETREnemyActionState::Launch, GetWorldTime(Context));

	// 정지 거리가 바뀌었으므로 옛 거리로 내린 도달 판정은 의미를 잃는다.
	Context.GetExternalData(TargetHandle).bArrived = false;

	FMassCommandBuffer& Commands = MassContext.GetMassEntityExecutionContext().Defer();
	Commands.RemoveTag<FTRArrivedTag>(MassContext.GetEntity());
	Commands.AddTag<FTRLaunchedTag>(MassContext.GetEntity());

	return EStateTreeRunStatus::Running;
}

bool FTRFireTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyStateHandle);
	Linker.LinkExternalData(CombatHandle);
	Linker.LinkExternalData(TargetHandle);
	Linker.LinkExternalData(ChannelHandle);
	Linker.LinkExternalData(CombatParamsHandle);
	Linker.LinkExternalData(CombatSubsystemHandle);
	return true;
}

void FTRFireTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FTREnemyStateFragment>();
	Builder.AddReadWrite<FTREnemyCombatFragment>();
	Builder.AddReadOnly<FTRTargetFragment>();
	Builder.AddReadOnly<FTRChaseChannelFragment>();
	Builder.AddReadOnly<FTREnemyCombatParams>();
}

EStateTreeRunStatus FTRFireTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	// 첫 발은 들어오자마자 나간다.
	Context.GetExternalData(CombatHandle).NextActionTime = 0.f;

	return Tick(Context, 0.f);
}

EStateTreeRunStatus FTRFireTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FTREnemyCombatParams& Params = Context.GetExternalData(CombatParamsHandle);
	FTREnemyCombatFragment& Combat = Context.GetExternalData(CombatHandle);

	if (Combat.ShotsFired >= Params.ShotCount)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const float WorldTime = GetWorldTime(Context);
	if (Combat.NextActionTime > 0.f && WorldTime < Combat.NextActionTime)
	{
		return EStateTreeRunStatus::Running;
	}

	// 착탄점은 사수 위치가 아니라 조준한 기차 표면이다.
	FTRMassImpactEvent Impact;
	Impact.Location = Context.GetExternalData(TargetHandle).TargetLocation;
	Impact.Damage = Combat.ShotDamage;
	Impact.Radius = UE::TR::Enemy::ShotImpactRadius;
	Impact.SourceType = ETREnemyType::Shooter;
	Impact.bExplosion = false;
	Impact.Channel = Context.GetExternalData(ChannelHandle).Channel;

	Context.GetExternalData(CombatSubsystemHandle).EnqueueImpact(Impact);

	++Combat.ShotsFired;
	Combat.NextActionTime = WorldTime + Params.FireInterval;
	EnterActionState(Context.GetExternalData(EnemyStateHandle), ETREnemyActionState::Fire, WorldTime);

	return (Combat.ShotsFired >= Params.ShotCount)
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

bool FTRDeathTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyStateHandle);
	return true;
}

void FTRDeathTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FTREnemyStateFragment>();
}

EStateTreeRunStatus FTRDeathTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	EnterActionState(Context.GetExternalData(EnemyStateHandle), ETREnemyActionState::Death, GetWorldTime(Context));

	// 연출 시간이 없으면 이번 틱에 바로 지운다.
	return Tick(Context, 0.f);
}

EStateTreeRunStatus FTRDeathTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FTREnemyStateFragment& State = Context.GetExternalData(EnemyStateHandle);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (GetWorldTime(Context) - State.StateEnteredTime < InstanceData.LingerDuration)
	{
		return EStateTreeRunStatus::Running;
	}

	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	MassContext.GetMassEntityExecutionContext().Defer().DestroyEntity(MassContext.GetEntity());

	return EStateTreeRunStatus::Succeeded;
}

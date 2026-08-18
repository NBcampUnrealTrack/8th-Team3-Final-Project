// TRStateTreeTasks.cpp

#include "Mass/StateTree/TRStateTreeTasks.h"

#include "Engine/World.h"
#include "MassCommandBuffer.h"
#include "MassMovementFragments.h"
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
	TryEnterActionState(Context.GetExternalData(EnemyStateHandle), ETREnemyActionState::None, GetWorldTime(Context));

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
	if (!TryEnterActionState(Context.GetExternalData(EnemyStateHandle), ETREnemyActionState::Attack, WorldTime))
	{
		// 프로세서가 같은 프레임에 먼저 걸었다. 준비 시간과 태그는 그쪽이 이미 처리했다.
		return EStateTreeRunStatus::Running;
	}

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

	if (!TryEnterActionState(Context.GetExternalData(EnemyStateHandle), ETREnemyActionState::Launch, GetWorldTime(Context)))
	{
		// 프로세서가 같은 프레임에 먼저 사출했다. ApplyLaunched를 두 번 걸면
		// 체력 상한이 다시 내려가므로 여기서 멈춘다.
		return EStateTreeRunStatus::Running;
	}

	UE::TR::Enemy::ApplyLaunched(Params, Combat, Approach);

	FMassCommandBuffer& Commands = MassContext.GetMassEntityExecutionContext().Defer();

	// 정지 거리가 바뀌었으므로 옛 거리로 내린 도달 판정은 의미를 잃는다.
	SetArrived(Commands, MassContext.GetEntity(), Context.GetExternalData(TargetHandle), false);

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
	Linker.LinkExternalData(SignalSubsystemHandle);
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
	// 죽은 뒤에도 시체가 잠시 남으므로, 그동안 계속 쏘지 않도록 여기서 끊는다.
	if (Context.GetExternalData(EnemyStateHandle).ActionState == ETREnemyActionState::Death)
	{
		return EStateTreeRunStatus::Failed;
	}

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

	// 한 발마다 다시 재생해야 하므로 재시작이다. 이미 Fire 상태여도 갱신한다.
	RetriggerActionState(Context.GetExternalData(EnemyStateHandle), ETREnemyActionState::Fire, WorldTime);

	if (Combat.ShotsFired >= Params.ShotCount)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// 다음 발사 시각에 깨어나도록 예약한다.
	//
	// 이게 없으면 이 Tick 은 두 번 다시 불리지 않는다 — 트리는 신호받은 엔티티만 틱하고,
	// Running 을 반환한 엔티티에는 재신호를 넣지 않기 때문이다. 사격형이 첫 발만 쏘고
	// 그 자리에 굳는다.
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	Context.GetExternalData(SignalSubsystemHandle).DelaySignalEntity(
		UE::Mass::Signals::NewStateTreeTaskRequired, MassContext.GetEntity(), Params.FireInterval);

	return EStateTreeRunStatus::Running;
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
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	// 이미 Death 라면 다른 경로(포탑 사격 등)가 먼저 죽인 것이다. 그때는 진입 시각을
	// 그대로 두어야 연출 시간이 그 순간부터 정확히 재진다.
	TryEnterActionState(Context.GetExternalData(EnemyStateHandle), ETREnemyActionState::Death, GetWorldTime(Context));

	// 이 태그가 조종·분리에서 빼주고, UTRMassDeathProcessor 가 집어갈 표시가 된다.
	// 속도는 그 프로세서가 매 프레임 확정하므로 여기서 손댈 필요가 없다.
	MassContext.GetMassEntityExecutionContext().Defer().AddTag<FTRDeadTag>(MassContext.GetEntity());

	// 삭제는 UTRMassDeathProcessor 가 한다. 여기서 직접 지우면 포탑 사격 경로와 연출 시간이
	// 갈라지고, 두 경로가 겹칠 때 같은 엔티티를 두 번 지우려 든다.
	// 트리를 깨우는 지연 신호도 필요 없어졌다 — 프로세서는 매 틱 시각만 비교하면 된다.
	return EStateTreeRunStatus::Succeeded;
}

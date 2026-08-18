// TRStateTreeConditions.cpp

#include "Mass/StateTree/TRStateTreeConditions.h"

#include "MassExecutionContext.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Mass/Core/TRMassFragments.h"

bool FTRHasArrivedCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TargetHandle);
	return true;
}

void FTRHasArrivedCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FTRTargetFragment>();
}

bool FTRHasArrivedCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FTRTargetFragment& Target = Context.GetExternalData(TargetHandle);
	const bool bArrived = Target.bHasTarget && Target.bArrived;

	return bInvert ? !bArrived : bArrived;
}

bool FTRIsEnemyTypeCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(CombatParamsHandle);
	return true;
}

void FTRIsEnemyTypeCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FTREnemyCombatParams>();
}

bool FTRIsEnemyTypeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FTREnemyCombatParams& Params = Context.GetExternalData(CombatParamsHandle);

	// **사출된 상체는 종류가 사출형이어도 행동은 근접형이다** — 접촉하면 터진다(기획서).
	//
	// 이 규칙을 조건 안에 두는 이유: 트리는 종류로 분기하는데, 사출 후에도 종류가
	// Launcher로 남아 있으면 Launch 가지로 다시 들어가 두 번째 사출을 시도하고,
	// 그건 이미 Launch 상태라 막히므로 상체가 기차에 닿고도 영원히 안 터진다.
	//
	// 종류를 여기서 한 번 바꿔주면 트리 애셋을 건드리지 않고 Melee 가지로 흘러간다.
	// 태그는 아키타입 단위라 실행 컨텍스트에 물어보는 것이 가장 싸다.
	// (엔티티 매니저로 아키타입을 조회해 비트셋을 꺼내는 경로도 되지만, 5.8에서
	//  부분 비트셋 게터가 deprecated 되었고 비용도 더 크다.)
	const FMassStateTreeExecutionContext& MassContext = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const bool bLaunched = MassContext.GetMassEntityExecutionContext().DoesArchetypeHaveTag<FTRLaunchedTag>();

	const ETREnemyType Behavior = bLaunched ? ETREnemyType::Melee : Params.EnemyType;
	const bool bMatches = (Behavior == EnemyType);

	return bInvert ? !bMatches : bMatches;
}

bool FTRAmmoDepletedCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(CombatHandle);
	Linker.LinkExternalData(CombatParamsHandle);
	return true;
}

void FTRAmmoDepletedCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FTREnemyCombatFragment>();
	Builder.AddReadOnly<FTREnemyCombatParams>();
}

bool FTRAmmoDepletedCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FTREnemyCombatFragment& Combat = Context.GetExternalData(CombatHandle);
	const FTREnemyCombatParams& Params = Context.GetExternalData(CombatParamsHandle);
	const bool bDepleted = (Combat.ShotsFired >= Params.ShotCount);

	return bInvert ? !bDepleted : bDepleted;
}

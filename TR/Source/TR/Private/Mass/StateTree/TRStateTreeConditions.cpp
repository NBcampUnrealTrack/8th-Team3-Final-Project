// TRStateTreeConditions.cpp

#include "Mass/StateTree/TRStateTreeConditions.h"

#include "MassStateTreeDependency.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

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
	const bool bMatches = (Params.EnemyType == EnemyType);

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

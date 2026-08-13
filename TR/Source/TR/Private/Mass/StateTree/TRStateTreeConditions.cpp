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

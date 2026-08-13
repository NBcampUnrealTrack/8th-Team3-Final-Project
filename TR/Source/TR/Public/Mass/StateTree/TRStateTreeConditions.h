// TRStateTreeConditions.h

#pragma once

#include "MassStateTreeTypes.h"
#include "Mass/Core/TRMassFragments.h"
#include "TRStateTreeConditions.generated.h"

struct FStateTreeExecutionContext;

USTRUCT()
struct FTRHasArrivedConditionInstanceData
{
	GENERATED_BODY()
};

/**
 * 목표에 도달했는지.
 *
 * 거리를 여기서 다시 계산하지 않는다. UTRChaseSteeringProcessor가 히스테리시스까지
 * 적용해 판정한 결과(FTRTargetFragment::bArrived)를 그대로 읽는다.
 * 같은 판정을 두 곳에서 하면 경계에서 서로 어긋난다.
 */
USTRUCT(meta = (DisplayName = "TR Has Arrived"))
struct FTRHasArrivedCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTRHasArrivedConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	TStateTreeExternalDataHandle<FTRTargetFragment> TargetHandle;

public:
	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};

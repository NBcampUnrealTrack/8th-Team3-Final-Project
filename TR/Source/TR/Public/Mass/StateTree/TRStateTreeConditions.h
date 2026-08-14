// TRStateTreeConditions.h

#pragma once

#include "MassStateTreeTypes.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Core/TRMassParams.h"
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

USTRUCT()
struct FTRIsEnemyTypeConditionInstanceData
{
	GENERATED_BODY()
};

/** 이 엔티티가 지정한 종류인지. 한 트리로 근접형·사출형·사격형을 나눌 때 쓴다. */
USTRUCT(meta = (DisplayName = "TR Is Enemy Type"))
struct FTRIsEnemyTypeCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTRIsEnemyTypeConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	TStateTreeExternalDataHandle<FTREnemyCombatParams> CombatParamsHandle;

public:
	UPROPERTY(EditAnywhere, Category = Condition)
	ETREnemyType EnemyType = ETREnemyType::Melee;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};

USTRUCT()
struct FTRAmmoDepletedConditionInstanceData
{
	GENERATED_BODY()
};

/** 사격형이 탄을 다 썼는지. 자체 폭발로 넘어가는 전이 조건이다. */
USTRUCT(meta = (DisplayName = "TR Ammo Depleted"))
struct FTRAmmoDepletedCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTRAmmoDepletedConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	TStateTreeExternalDataHandle<FTREnemyCombatFragment> CombatHandle;
	TStateTreeExternalDataHandle<FTREnemyCombatParams> CombatParamsHandle;

public:
	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};

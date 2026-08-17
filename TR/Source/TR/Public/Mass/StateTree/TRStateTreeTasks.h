// TRStateTreeTasks.h

#pragma once

#include "MassMovementFragments.h"
#include "MassStateTreeTypes.h"
#include "Mass/Combat/TRMassCombatSubsystem.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Core/TRMassParams.h"
#include "TRStateTreeTasks.generated.h"

struct FStateTreeExecutionContext;
class UMassSignalSubsystem;

// 주의: 커스텀 신호 이름을 만들어 쓸 수 없다.
// UMassStateTreeProcessor는 생성 시점에 정해진 엔진 신호 목록에만 구독하므로
// (StateTreeActivate / NewStateTreeTaskRequired / StandTaskFinished / ...),
// 그 목록에 없는 이름으로 신호를 보내면 트리가 깨어나지 않는다.
// 재평가 요청에는 UE::Mass::Signals::NewStateTreeTaskRequired 를 쓴다.

//----------------------------------------------------------------------//
//  MoveToTarget
//----------------------------------------------------------------------//

USTRUCT()
struct FTRMoveToTargetTaskInstanceData
{
	GENERATED_BODY()
};

/**
 * 목표를 향해 이동하는 상태.
 *
 * 이 태스크는 **이동 계산을 하지 않는다.** 행동 상태만 기록하고 끝난다.
 * 실제 이동은 UTRTargetSyncProcessor와 UTRChaseSteeringProcessor가 청크 단위로 처리한다.
 * 태스크 안에서 개체별 계산을 돌리면 Mass를 쓰는 의미가 사라진다.
 */
USTRUCT(meta = (DisplayName = "TR Move To Target"))
struct FTRMoveToTargetTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTRMoveToTargetTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FTREnemyStateFragment> EnemyStateHandle;
};

//----------------------------------------------------------------------//
//  Detonate
//----------------------------------------------------------------------//

USTRUCT()
struct FTRDetonateTaskInstanceData
{
	GENERATED_BODY()

	/**
	 * 폭발까지의 준비 시간(초). 플레이어가 반응할 여지를 주는 값이다.
	 * 이 태스크로 폭발을 걸면 이 값이 FTREnemyCombatParams::DetonationWindup보다 우선한다.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "s"))
	float WindupDuration = 0.4f;
};

/**
 * 자폭 예약.
 *
 * **여기서 터뜨리지 않는다.** 준비 시간을 기록하고 FTRDetonatingTag만 붙인다.
 * 실제 폭발(피해 큐 삽입 + 엔티티 제거)은 UTREnemyAttackProcessor가 한다.
 *
 * 폭발의 주인이 둘이면 안 되기 때문이다. StateTree를 붙인 적과 붙이지 않은 적이
 * 섞여 있어도 결과가 같아야 하고, 트리를 잘못 구성했을 때 "적이 기차에 붙은 채로
 * 영원히 안 터지는" 상태가 생기면 안 된다. 그래서 프로세서가 단독 실행자이고,
 * 이 태스크는 연출 타이밍을 트리에서 조정하기 위한 입구로만 남는다.
 */
USTRUCT(meta = (DisplayName = "TR Detonate"))
struct FTRDetonateTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTRDetonateTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FTREnemyStateFragment> EnemyStateHandle;
	TStateTreeExternalDataHandle<FTREnemyCombatFragment> CombatHandle;
};

USTRUCT()
struct FTRLaunchTaskInstanceData
{
	GENERATED_BODY()
};

/**
 * 사출형이 상체를 쏘아 보낸다. 새 엔티티를 만들지 않고 이 엔티티의 성격을 바꾼다.
 *
 * 기본 동작은 UTREnemyAttackProcessor 가 목표 거리에서 자동으로 한다.
 * 이 태스크는 사출 시점을 트리에서 앞당기거나 늦추려 할 때 쓴다.
 */
USTRUCT(meta = (DisplayName = "TR Launch"))
struct FTRLaunchTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTRLaunchTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FTREnemyStateFragment> EnemyStateHandle;
	TStateTreeExternalDataHandle<FTREnemyCombatFragment> CombatHandle;
	TStateTreeExternalDataHandle<FTRApproachFragment> ApproachHandle;
	TStateTreeExternalDataHandle<FTRTargetFragment> TargetHandle;
	TStateTreeExternalDataHandle<FTREnemyCombatParams> CombatParamsHandle;
};

USTRUCT()
struct FTRFireTaskInstanceData
{
	GENERATED_BODY()
};

/**
 * 사격형이 유지 거리에서 조준한 표면을 쏜다. 탄을 다 쓰면 Succeeded 로 끝난다.
 *
 * 발사 간격·탄 수·피해는 FTREnemyCombatParams 를 쓴다.
 */
USTRUCT(meta = (DisplayName = "TR Fire"))
struct FTRFireTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTRFireTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FTRFireTask() { bShouldCallTick = true; }

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FTREnemyStateFragment> EnemyStateHandle;
	TStateTreeExternalDataHandle<FTREnemyCombatFragment> CombatHandle;
	TStateTreeExternalDataHandle<FTRTargetFragment> TargetHandle;
	TStateTreeExternalDataHandle<FTRChaseChannelFragment> ChannelHandle;
	TStateTreeExternalDataHandle<FTREnemyCombatParams> CombatParamsHandle;
	TStateTreeExternalDataHandle<UTRMassCombatSubsystem> CombatSubsystemHandle;

	/**
	 * 다음 발사 시각에 스스로 깨어나기 위해 필요하다.
	 *
	 * UMassStateTreeProcessor 는 **신호를 받은 엔티티만** 트리를 틱한다. 그리고 태스크가
	 * Running 을 반환하면 재신호를 넣지 않는다(엔진 코드 확인). 즉 이 Tick 은 가만히
	 * 두면 두 번 다시 불리지 않고, 사격형은 첫 발만 쏘고 굳는다.
	 */
	TStateTreeExternalDataHandle<UMassSignalSubsystem> SignalSubsystemHandle;
};

USTRUCT()
struct FTRDeathTaskInstanceData
{
	GENERATED_BODY()
};

/**
 * 사망 상태로 들어간다.
 *
 * 시체를 얼마나 남길지는 FTREnemyCombatParams::DeathLinger 가 정하고, 실제 삭제는
 * UTRMassDeathProcessor 가 한다. 죽는 경로가 여럿이라 삭제 시점은 한 곳에 모아야 한다.
 */
USTRUCT(meta = (DisplayName = "TR Death"))
struct FTRDeathTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTRDeathTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FTREnemyStateFragment> EnemyStateHandle;

	/**
	 * 죽는 순간 속도를 끊기 위해.
	 *
	 * 태그만 붙여서는 부족하다. 태그 부착은 명령 버퍼를 거쳐 다음 플러시에 반영되고,
	 * 그 사이에도 엔진 이동 프로세서는 남아 있는 속도로 계속 엔티티를 옮긴다.
	 */
	TStateTreeExternalDataHandle<FMassVelocityFragment> VelocityHandle;
	TStateTreeExternalDataHandle<FMassForceFragment> ForceHandle;
};

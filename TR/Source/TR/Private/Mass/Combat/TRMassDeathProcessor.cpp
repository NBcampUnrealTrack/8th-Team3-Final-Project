// TRMassDeathProcessor.cpp

#include "Mass/Combat/TRMassDeathProcessor.h"

#include "Engine/World.h"
#include "MassCommandBuffer.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "Movement/MassMovementProcessors.h"
#include "Environment/TRGroundMotionSubsystem.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Core/TRMassParams.h"
#include "Mass/Debug/TRMassDebug.h"

DECLARE_CYCLE_STAT(TEXT("Enemy Death"), STAT_TRMassDeath, STATGROUP_TRMass);

UTRMassDeathProcessor::UTRMassDeathProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// 엔티티 삭제는 서버 권위. 클라이언트는 복제된 표현만 본다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;

	// 힘이 속도로 바뀐 **뒤**, 그 속도가 트랜스폼에 적용되기 **전**.
	// 이 사이가 시체의 속도를 확정할 수 있는 유일한 지점이다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::ApplyForces);
	ExecutionOrder.ExecuteBefore.Add(UMassApplyMovementProcessor::StaticClass()->GetFName());
}

void UTRMassDeathProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTREnemyStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FTREnemyCombatParams>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FTRDeadTag>(EMassFragmentPresence::All);

	/**
	 * **DesiredVelocity 가 이동의 진짜 상태다.**
	 *
	 * UMassApplyMovementProcessor 가 매 프레임 `Velocity = DesiredVelocity` 로 덮은 뒤
	 * 그 값으로 트랜스폼을 옮긴다. 그래서 FMassVelocityFragment 를 지우는 것은 의미가 없고
	 * (다음 줄에서 되살아난다), FMassForceFragment 도 UMassApplyForceProcessor 가 적용 직후
	 * 스스로 0으로 되돌리므로 건드릴 필요가 없다. 여기 하나만 쓰면 된다.
	 */
	EntityQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);

	// 시체는 지면과 함께 흘러가야 한다. 스스로 달리는 산 개체와 정반대다.
	EntityQuery.AddSubsystemRequirement<UTRGroundMotionSubsystem>(EMassFragmentAccess::ReadOnly);
}

void UTRMassDeathProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRMassDeath);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRMassDeath);

	const UWorld* World = EntityManager.GetWorld();
	const float WorldTime = World ? World->GetTimeSeconds() : 0.f;

	EntityQuery.ForEachEntityChunk(Context, [WorldTime](FMassExecutionContext& Context)
	{
		const FTREnemyCombatParams& Params = Context.GetConstSharedFragment<FTREnemyCombatParams>();
		const TConstArrayView<FTREnemyStateFragment> StateList = Context.GetFragmentView<FTREnemyStateFragment>();

		// 지면이 멈춘 레벨에서는 0이라 시체가 그 자리에 선다. 분기가 따로 필요 없다.
		const FVector GroundVelocity =
			Context.GetSubsystemChecked<UTRGroundMotionSubsystem>().GetGroundVelocity();

		const TArrayView<FMassDesiredMovementFragment> DesiredList =
			Context.GetMutableFragmentView<FMassDesiredMovementFragment>();
		const bool bHasDesired = (DesiredList.Num() > 0);

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			// 매 프레임 덮는다. 죽기 직전의 값은 아무도 지워주지 않아 그대로 남고,
			// 0으로 두면 시체가 기차와 같은 거리를 유지한 채 발밑 지면만 흘러간다.
			if (bHasDesired)
			{
				DesiredList[EntityIt].DesiredVelocity = GroundVelocity;
			}

			// 진입 시각은 죽은 순간이다. 표현이 사망 클립의 재생 위치를 재는 것과 같은 원점이라,
			// 연출이 끝나는 시점과 시체가 사라지는 시점이 저절로 맞는다.
			const float Elapsed = WorldTime - StateList[EntityIt].StateEnteredTime;
			if (Elapsed >= Params.DeathLinger)
			{
				Context.Defer().DestroyEntity(EntityIt.GetEntityHandle());
			}
		}
	});
}

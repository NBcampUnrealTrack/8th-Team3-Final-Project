// TRMassActorSyncProcessor.cpp

#include "Mass/Representation/TRMassActorSyncProcessor.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Representation/TRMassEnemyActor.h"
#include "Mass/Core/TRMassFragments.h"

DECLARE_CYCLE_STAT(TEXT("Actor Sync"), STAT_TRMassActorSync, STATGROUP_TRMass);

UTRMassActorSyncProcessor::UTRMassActorSyncProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// 표현은 클라이언트에서도 돌아야 하므로 넷 모드를 가리지 않는다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;

	// 시뮬레이션이 끝난 뒤 그 결과를 월드에 반영하는 단계.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;

	// AActor::SetActorTransform은 게임 스레드 전용이다.
	bRequiresGameThreadExecution = true;
}

void UTRMassActorSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FTREnemyStateFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);
}

void UTRMassActorSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRMassActorSync);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRMassActorSync);

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	const UWorld* World = EntityManager.GetWorld();
	const float WorldTime = World ? World->GetTimeSeconds() : 0.f;

	EntityQuery.ForEachEntityChunk(Context, [DeltaTime, WorldTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassActorFragment> ActorList = Context.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FTREnemyStateFragment> StateList = Context.GetFragmentView<FTREnemyStateFragment>();

		const bool bHasVelocity = (VelocityList.Num() > 0);
		const bool bHasState = (StateList.Num() > 0);

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			// Mass가 스폰한 표현 액터만 움직인다. 외부에서 붙인 액터는 자기 트랜스폼의 주인이므로 건드리지 않는다.
			AActor* Actor = ActorList[EntityIt].GetOwnedByMassMutable();
			if (Actor == nullptr)
			{
				continue;
			}

			// 물리 계산 없이 위치만 옮긴다. 표현용이라 충돌도 꺼져 있다.
			Actor->SetActorTransform(TransformList[EntityIt].GetTransform(),
				/*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);

			// 애니메이션에 필요한 값을 직접 넘긴다. 액터가 위치 차분으로 역산하면 틱이 필요하고,
			// LOD 전환/풀 재사용으로 순간이동할 때 값이 튄다.
			if (ATRMassEnemyActor* EnemyActor = Cast<ATRMassEnemyActor>(Actor))
			{
				const FVector Velocity = bHasVelocity ? VelocityList[EntityIt].Value : FVector::ZeroVector;

				ETREnemyActionState ActionState = ETREnemyActionState::None;
				float StateElapsed = 0.f;
				if (bHasState)
				{
					ActionState = StateList[EntityIt].ActionState;
					StateElapsed = WorldTime - StateList[EntityIt].StateEnteredTime;
				}

				EnemyActor->SetMassState(Velocity, ActionState, StateElapsed, DeltaTime);
			}
		}
	});
}

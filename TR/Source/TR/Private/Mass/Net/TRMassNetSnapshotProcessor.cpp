// TRMassNetSnapshotProcessor.cpp

#include "Mass/Net/TRMassNetSnapshotProcessor.h"

#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Net/TRMassNetRelay.h"

DECLARE_CYCLE_STAT(TEXT("Net Snapshot"), STAT_TRMassNetSnapshot, STATGROUP_TRMass);

UTRMassNetSnapshotProcessor::UTRMassNetSnapshotProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// 보낼 것이 있는 쪽은 서버뿐이다. 스탠드얼론은 받을 상대가 없다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server;

	// 시뮬레이션 결과를 월드 밖으로 내보내는 단계.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;

	// 액터(릴레이)를 만진다.
	bRequiresGameThreadExecution = true;
}

void UTRMassNetSnapshotProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTRMassNetIdFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTREnemyStateFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);
}

void UTRMassNetSnapshotProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRMassNetSnapshot);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRMassNetSnapshot);

	UWorld* World = EntityManager.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	ATRMassNetRelay* Relay = CachedRelay.Get();
	if (Relay == nullptr)
	{
		Relay = ATRMassNetRelay::Get(World, /*bCreateIfMissing=*/true);
		CachedRelay = Relay;

		if (Relay == nullptr)
		{
			return;
		}
	}

	const float WorldTime = World->GetTimeSeconds();
	if (WorldTime < NextSnapshotTime)
	{
		return;
	}
	NextSnapshotTime = WorldTime + Relay->GetSnapshotInterval();

	TArray<FTRMassAgentSnapshot> Snapshot;

	EntityQuery.ForEachEntityChunk(Context, [&Snapshot](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTRMassNetIdFragment> NetIdList = Context.GetFragmentView<FTRMassNetIdFragment>();
		const TConstArrayView<FTREnemyStateFragment> StateList = Context.GetFragmentView<FTREnemyStateFragment>();

		const bool bHasState = (StateList.Num() > 0);

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTRMassNetIdFragment& NetId = NetIdList[EntityIt];
			if (NetId.NetId == 0)
			{
				// 아직 번호를 못 받은 개체. 스폰과 같은 프레임일 때 잠깐 생긴다.
				continue;
			}

			const FTransform& Transform = TransformList[EntityIt].GetTransform();

			FTRMassAgentSnapshot& Agent = Snapshot.AddDefaulted_GetRef();
			Agent.NetId = NetId.NetId;
			Agent.ConfigIndex = NetId.ConfigIndex;
			Agent.Location = Transform.GetLocation();

			// Yaw 를 1바이트로. 음수 각도도 감싸도록 0~360 으로 정규화한 뒤 나눈다.
			const float Yaw = FRotator::ClampAxis(Transform.GetRotation().Rotator().Yaw);
			Agent.Yaw = static_cast<uint8>(FMath::RoundToInt(Yaw * (256.f / 360.f))) ;

			Agent.ActionState = bHasState ? StateList[EntityIt].ActionState : ETREnemyActionState::None;
		}
	});

	Relay->SubmitSnapshot(MoveTemp(Snapshot));
}

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

	// 손상도 복제용. 클라이언트에는 전투 프로세서가 없어 이 값이 유일한 출처다.
	EntityQuery.AddRequirement<FTREnemyVisualFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
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
		const TConstArrayView<FTREnemyVisualFragment> VisualList = Context.GetFragmentView<FTREnemyVisualFragment>();

		const bool bHasState = (StateList.Num() > 0);
		const bool bHasVisual = (VisualList.Num() > 0);

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTRMassNetIdFragment& NetId = NetIdList[EntityIt];
			if (NetId.NetId == 0)
			{
				// 아직 번호를 못 받은 개체. 스폰과 같은 프레임일 때 잠깐 생긴다.
				continue;
			}

			// 무엇을 어떻게 눌러 담을지는 스냅샷 구조체가 안다. 이 프로세서는 "어느
			// 엔티티를" 만 고른다 — 굽는 규칙이 되돌리는 규칙 바로 옆에 있어야
			// 필드를 늘릴 때 한쪽만 고치는 일이 줄어든다(FTRMassAgentSnapshot 주석 참고).
			FTRMassAgentSnapshot& Agent = Snapshot.AddDefaulted_GetRef();
			Agent.CollectFrom(
				TransformList[EntityIt].GetTransform(),
				NetId,
				bHasState ? &StateList[EntityIt] : nullptr,
				bHasVisual ? &VisualList[EntityIt] : nullptr);
		}
	});

	Relay->SubmitSnapshot(MoveTemp(Snapshot));
}

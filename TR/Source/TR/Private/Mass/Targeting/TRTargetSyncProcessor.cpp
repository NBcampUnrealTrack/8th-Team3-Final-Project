// TRTargetSyncProcessor.cpp

#include "Mass/Targeting/TRTargetSyncProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Targeting/TRTargetDirectorSubsystem.h"

DECLARE_CYCLE_STAT(TEXT("Target Sync"), STAT_TRTargetSync, STATGROUP_TRMass);

UTRTargetSyncProcessor::UTRTargetSyncProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	// 목표 판단은 서버 권위. 클라이언트는 복제된 표현만 본다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
}

void UTRTargetSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTRChaseChannelFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTRTargetFragment>(EMassFragmentAccess::ReadWrite);
	// 표면 최근접점은 "내가 지금 어디 있는가"에 따라 달라진다.
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);

	// 이 선언이 있어야 프로세서 안에서 서브시스템에 접근할 수 있다.
	EntityQuery.AddSubsystemRequirement<UTRTargetDirectorSubsystem>(EMassFragmentAccess::ReadOnly);
}

void UTRTargetSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRTargetSync);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRTargetSync);

	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const UTRTargetDirectorSubsystem& Director = Context.GetSubsystemChecked<UTRTargetDirectorSubsystem>();

		// 채널별 목표를 청크당 한 번만 해석한다. 엔티티 루프에서는 형상에 대한 클램프만 한다.
		FTRResolvedTarget Resolved[static_cast<int32>(ETRTargetChannel::MAX)];
		for (int32 ChannelIndex = 0; ChannelIndex < UE_ARRAY_COUNT(Resolved); ++ChannelIndex)
		{
			Director.GetResolvedTarget(static_cast<ETRTargetChannel>(ChannelIndex), Resolved[ChannelIndex]);
		}

		const TConstArrayView<FTRChaseChannelFragment> ChannelList = Context.GetFragmentView<FTRChaseChannelFragment>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FTRTargetFragment> TargetList = Context.GetMutableFragmentView<FTRTargetFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			// 채널 값은 스폰 파라미터에서 오므로 블루프린트가 바이트 캐스트로 범위를 벗어난
			// 값을 넣을 수 있다. 그러면 스택 배열을 벗어나 읽는다.
			const int32 ChannelIndex = static_cast<int32>(ChannelList[EntityIt].Channel);
			if (ChannelIndex < 0 || ChannelIndex >= UE_ARRAY_COUNT(Resolved))
			{
				TargetList[EntityIt].bHasTarget = false;
				continue;
			}

			if (!Resolved[ChannelIndex].bValid)
			{
				TargetList[EntityIt].bHasTarget = false;
				continue;
			}

			const FTRResolvedTarget& Entry = Resolved[ChannelIndex];
			FTRTargetFragment& Target = TargetList[EntityIt];
			const FVector SelfLocation = TransformList[EntityIt].GetTransform().GetLocation();

			// 장갑·포탑 구역만 노린다. 등급도 파괴 여부도 보지 않고 가장 가까운 것을 고른다.
			// 개체마다 다른 점을 향하므로 자연스럽게 기차를 둘러싸는 모양이 된다.
			// 최근접점은 내 위치에 따라 달라지므로 매 틱 다시 구한다.
			FVector ZonePoint;
			if (!Entry.TryGetClosestZonePoint(SelfLocation, ZonePoint))
			{
				Target.bHasTarget = false;
				continue;
			}

			Target.TargetLocation = ZonePoint;
			Target.bHasTarget = true;

			// 도달 상태(bArrived)와 태그는 건드리지 않는다. 그 전이는 Steering 프로세서가
			// 단독으로 소유해야 태그와 bArrived가 어긋나지 않는다. 목표가 교체돼 멀어지면
			// Steering이 다음 틱에 알아서 도달 상태를 푼다.
		}
	});
}

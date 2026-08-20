// TRMassRadarProcessor.cpp

#include "Mass/Radar/TRMassRadarProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Core/TRMassParams.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Radar/TRRadarSubsystem.h"

DECLARE_CYCLE_STAT(TEXT("Radar Gather"), STAT_TRMassRadarGather, STATGROUP_TRMass);

UTRMassRadarProcessor::UTRMassRadarProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// 표현과 마찬가지로 넷 모드를 가리지 않는다. 클라이언트의 로컬 엔티티도
	// 릴레이가 보간해 둔 정확한 위치를 갖고 있으므로 같은 코드가 그대로 맞다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;

	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;

	// 서브시스템의 배열 하나에 모든 청크가 모여 쓴다. 락을 두느니 게임 스레드로
	// 못 박는 편이 낫다 — 120개 좌표 복사라 병렬화할 만한 일이 애초에 없다.
	bRequiresGameThreadExecution = true;
}

void UTRMassRadarProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);

	// 시체는 찍지 않는다. 쓰러진 자리에 남아 있는 동안 레이더가 위협처럼 보이면
	// 플레이어가 이미 정리된 방향으로 포탑을 돌린다.
	EntityQuery.AddTagRequirement<FTRDeadTag>(EMassFragmentPresence::None);

	// 종류는 공유 프래그먼트라 청크당 한 번만 읽으면 된다. Optional 인 이유는
	// 전투 파라미터가 없는 개체(연출용 등)도 위치는 찍혀야 하기 때문이다.
	EntityQuery.AddConstSharedRequirement<FTREnemyCombatParams>(EMassFragmentPresence::Optional);

	EntityQuery.AddSubsystemRequirement<UTRRadarSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UTRMassRadarProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRMassRadarGather);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRMassRadarGather);

	UTRRadarSubsystem* Radar = EntityManager.GetWorld()
		? EntityManager.GetWorld()->GetSubsystem<UTRRadarSubsystem>()
		: nullptr;

	if (Radar == nullptr)
	{
		return;
	}

	// 청크가 하나도 없는 프레임(=적 전멸)에도 반드시 불려야 한다. 그래서
	// ForEachEntityChunk 안이 아니라 여기서 비운다.
	Radar->BeginGather();

	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

		const FTREnemyCombatParams* CombatParams = Context.GetConstSharedFragmentPtr<FTREnemyCombatParams>();
		const ETREnemyType Type = (CombatParams != nullptr) ? CombatParams->EnemyType : ETREnemyType::Melee;

		UTRRadarSubsystem& Radar = Context.GetMutableSubsystemChecked<UTRRadarSubsystem>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			Radar.AddBlip(TransformList[EntityIt].GetTransform().GetLocation(), Type);
		}
	});
}

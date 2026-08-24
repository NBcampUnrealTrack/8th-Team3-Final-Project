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
	//
	// 판정을 FTRDeadTag 가 아니라 행동 상태로 하는 이유는 두 가지다
	// (UTRMassSpawnerSubsystem 의 동시 상한 계산도 같은 이유로 상태를 본다).
	//
	//  1. **태그는 클라이언트에 아예 없다.** 태그를 붙이는 곳은 전투 서브시스템과
	//     StateTree 태스크 둘뿐인데 양쪽 다 서버 전용 경로다(엔진이 StateTree 를
	//     Standalone|Server 로 박아 두었다). 이 프로세서는 AllNetModes 라 클라이언트에서도
	//     도는데, 거기서는 태그가 영원히 안 붙어 필터가 통과만 한다 — 호스트에서는
	//     즉시 사라지고 클라이언트 레이더에만 시체가 DeathLinger 동안 남는다.
	//  2. 태그는 명령 버퍼를 거쳐 한 프레임 늦게 붙는다. 상태는 죽는 즉시 바뀌므로
	//     서버에서도 그 한 프레임의 오차가 없어진다.
	//
	// 상태 프래그먼트가 없는 구성(TR Chaser 트레이트를 안 쓴 Config)은 살아 있는 것으로 본다.
	EntityQuery.AddRequirement<FTREnemyStateFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

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
		const TConstArrayView<FTREnemyStateFragment> StateList = Context.GetFragmentView<FTREnemyStateFragment>();

		const bool bHasState = (StateList.Num() > 0);

		const FTREnemyCombatParams* CombatParams = Context.GetConstSharedFragmentPtr<FTREnemyCombatParams>();
		const ETREnemyType Type = (CombatParams != nullptr) ? CombatParams->EnemyType : ETREnemyType::Melee;

		UTRRadarSubsystem& Radar = Context.GetMutableSubsystemChecked<UTRRadarSubsystem>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			if (bHasState && StateList[EntityIt].ActionState == ETREnemyActionState::Death)
			{
				continue;
			}

			Radar.AddBlip(TransformList[EntityIt].GetTransform().GetLocation(), Type);
		}
	});
}

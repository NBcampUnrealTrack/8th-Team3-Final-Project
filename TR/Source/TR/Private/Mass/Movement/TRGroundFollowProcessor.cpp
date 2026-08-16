// TRGroundFollowProcessor.cpp

#include "Mass/Movement/TRGroundFollowProcessor.h"

#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassSimulationLOD.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Core/TRMassParams.h"
#include "Mass/Core/TRMassGroundQuery.h"

DECLARE_CYCLE_STAT(TEXT("Ground Follow"), STAT_TRGroundFollow, STATGROUP_TRMass);

namespace
{
	/** 트레이스 시작점을 발밑보다 이만큼 위에서 잡는다. 계단 오르내림을 흡수한다. */
	constexpr float TraceUpOffset = 200.f;

	/** 아래로 이만큼까지 찾는다. 못 찾으면 지면이 없다고 본다. */
	constexpr float TraceDownOffset = 2000.f;
}

UTRGroundFollowProcessor::UTRGroundFollowProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;

	// 이동 적분이 끝난 뒤 최종 위치를 보정한다.
	//
	// ExecuteInGroup을 Movement로 두면 안 된다. 자기가 속한 그룹 뒤에 실행하라는
	// 뜻이 되어 자기 자신에 대한 의존성이 생기고, 의존성 해석기가 어서션으로 죽는다.
	// 그룹에 속하지 않은 채 순서만 지정하는 것이 엔진의 동등한 프로세서
	// (UMassNavigationSmoothHeightProcessor)가 쓰는 방식이다.
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);

	// 보정된 위치를 액터/인스턴스가 읽어가야 하므로 표현 반영보다 먼저 끝낸다.
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);

	// 씬 쿼리는 게임 스레드에서 수행한다.
	bRequiresGameThreadExecution = true;
}

void UTRGroundFollowProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTRGroundFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);

	// 사출된 상체는 날아가는 중이라 지면을 따르면 안 된다. 따르게 두면 25 m/s로
	// 땅을 훑고 지나가서 투사체로 보이지 않는다.
	EntityQuery.AddTagRequirement<FTRLaunchedTag>(EMassFragmentPresence::None);

	EntityQuery.AddConstSharedRequirement<FTRChaseParams>(EMassFragmentPresence::All);

	// 청크 필터로 원거리 개체는 프레임을 건너뛴다. 그 경우 이번 틱이 실제로 대표하는
	// 시간은 월드 델타가 아니라 **건너뛴 만큼 누적된 값**이고, 그건 이 프래그먼트에 있다.
	// 월드 델타로 보간하면 LOD가 낮은 개체일수록 지면을 늦게 따라와 경사에서 뜨거나 잠긴다.
	// (UTRChaseSteeringProcessor가 같은 이유로 같은 값을 읽는다.)
	EntityQuery.AddRequirement<FMassSimulationVariableTickFragment>(
		EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddChunkRequirement<FMassSimulationVariableTickChunkFragment>(
		EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.SetChunkFilter(&FMassSimulationVariableTickChunkFragment::ShouldTickChunkThisFrame);
}

void UTRGroundFollowProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRGroundFollow);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRGroundFollow);

	UWorld* World = EntityManager.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float WorldTime = World->GetTimeSeconds();
	const float WorldDeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [World, WorldTime, WorldDeltaTime](FMassExecutionContext& Context)
	{
		const FTRChaseParams& ChaseParams = Context.GetConstSharedFragment<FTRChaseParams>();
		if (!ChaseParams.bFollowGround)
		{
			return;
		}

		const TConstArrayView<FMassSimulationVariableTickFragment> VariableTickList =
			Context.GetFragmentView<FMassSimulationVariableTickFragment>();
		const bool bHasVariableTick = (VariableTickList.Num() > 0);

		const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FTRGroundFragment> GroundList = Context.GetMutableFragmentView<FTRGroundFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FTransform& Transform = TransformList[EntityIt].GetMutableTransform();
			FTRGroundFragment& Ground = GroundList[EntityIt];

			// 큰 시간 간격에서 보간이 튀는 걸 막는다(엔진 이동 프로세서와 동일한 상한).
			const float DeltaTime = FMath::Min(0.1f,
				bHasVariableTick ? VariableTickList[EntityIt].DeltaTime : WorldDeltaTime);

			FVector Location = Transform.GetLocation();

			if (WorldTime >= Ground.NextTraceTime)
			{
				// 다음 트레이스 시각을 개체마다 조금씩 다르게 잡아 한 프레임에 몰리지 않게 한다.
				// 전부 같은 주기로 맞으면 주기마다 프레임이 튄다.
				const float Jitter = FMath::FRandRange(0.f, ChaseParams.GroundTraceInterval * 0.25f);
				Ground.NextTraceTime = WorldTime + ChaseParams.GroundTraceInterval + Jitter;

				// 지면을 못 찾으면 현재 높이를 유지한다. 억지로 떨어뜨리면
				// 다리 위나 절벽 가장자리에서 순간이동한 것처럼 보인다.
				Ground.bHasGround = UE::TR::MassGround::FindGroundZBelow(
					*World, Location, TraceUpOffset, TraceDownOffset, Ground.GroundZ);
			}

			if (!Ground.bHasGround)
			{
				continue;
			}

			// 트레이스 사이 구간은 보간으로 메운다. 주기마다 계단처럼 튀는 걸 막는다.
			Location.Z = (ChaseParams.GroundSmoothingTime > 0.f)
				? FMath::FInterpTo(Location.Z, Ground.GroundZ, DeltaTime, 1.f / ChaseParams.GroundSmoothingTime)
				: Ground.GroundZ;

			Transform.SetLocation(Location);
		}
	});
}

// TRChaseSeparationProcessor.cpp

#include "Mass/Movement/TRChaseSeparationProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassLODFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavigationSubsystem.h"
#include "MassSimulationLOD.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Core/TRMassParams.h"

DECLARE_CYCLE_STAT(TEXT("Chase Separation"), STAT_TRChaseSeparation, STATGROUP_TRMass);

namespace
{
	/**
	 * 그리드 질의 결과를 담을 인라인 버퍼 크기.
	 *
	 * TFixedAllocator를 쓰면 안 된다. 엔진의 Query()는 결과 수에 상한이 없어서
	 * 개체가 한곳에 뭉치면 이 크기를 넘고, 그 순간 assert로 죽는다.
	 * TInlineAllocator는 넘칠 때 힙으로 넘어가므로 밀집 상황에서도 안전하다.
	 * (엔진 회피 코드가 TFixedAllocator를 쓰는 건 자체 순회에서 직접 잘라내기 때문이다.)
	 */
	constexpr int32 GridQueryInlineCapacity = 64;
}

UTRChaseSeparationProcessor::UTRChaseSeparationProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;

	// Steering과 같은 그룹. 둘 다 힘을 누적하기만 하므로 순서는 상관없고,
	// ApplyForces 그룹이 이 뒤에서 합산 결과를 속도로 바꾼다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
}

void UTRChaseSeparationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);

	EntityQuery.AddConstSharedRequirement<FTRChaseParams>(EMassFragmentPresence::All);

	// Off LOD를 제외하지 않는다.
	//
	// 처음엔 "화면 밖은 겹쳐 보일 일이 없다"고 보고 건너뛰었는데, 시뮬레이션 LOD 거리와
	// 시각화 LOD 거리가 서로 독립이라 **시뮬레이션상 Off인데 화면에는 보이는** 구간이 생긴다.
	// 그 구간의 개체들이 분리 없이 겹친 채로 보인다.
	//
	// 비용은 아래 가변 틱으로 줄인다. 기능을 끄는 게 아니라 빈도를 낮추는 쪽이라
	// 거리 설정이 어긋나도 눈에 띄는 결함으로 이어지지 않는다.
	// 청크 프래그먼트가 없으면 필터가 항상 true라 트레이트 없이도 동작한다.
	EntityQuery.AddChunkRequirement<FMassSimulationVariableTickChunkFragment>(
		EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.SetChunkFilter(&FMassSimulationVariableTickChunkFragment::ShouldTickChunkThisFrame);

	// 그리드는 이 서브시스템이 소유한다. 읽기만 한다.
	EntityQuery.AddSubsystemRequirement<UMassNavigationSubsystem>(EMassFragmentAccess::ReadOnly);
}

void UTRChaseSeparationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRChaseSeparation);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRChaseSeparation);

	EntityQuery.ForEachEntityChunk(Context, [&EntityManager](FMassExecutionContext& Context)
	{
		const FTRChaseParams& ChaseParams = Context.GetConstSharedFragment<FTRChaseParams>();
		if (ChaseParams.SeparationStrength <= 0.f)
		{
			return;
		}

		const FNavigationObstacleHashGrid2D& Grid =
			Context.GetSubsystemChecked<UMassNavigationSubsystem>().GetObstacleGrid();

		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
		const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();

		TArray<FMassNavigationObstacleItem, TInlineAllocator<GridQueryInlineCapacity>> Neighbors;

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle SelfEntity = EntityIt.GetEntityHandle();
			const FVector SelfLocation = TransformList[EntityIt].GetTransform().GetLocation();
			const float SelfRadius = RadiusList[EntityIt].Radius;

			// 밀어낼 거리. 질의 범위이자 판정 반경이다.
			//
			// 이웃 반지름을 따로 조회하지 않고 자기 반지름을 두 배로 쓴다. 임의 접근
			// 조회는 캐시를 크게 흔들어서 이 프로세서에서 가장 비싼 부분인데, 같은 종류의
			// 적은 반지름이 같으므로 결과가 사실상 동일하다. 크기가 크게 다른 적을 섞을
			// 때는 이 근사를 다시 봐야 한다.
			//
			// 질의는 축 정렬 박스라 이 반경의 원을 완전히 덮는다. 놓치는 이웃은 없다.
			const float SeparationRadius = SelfRadius * 2.f * ChaseParams.SeparationRadiusScale;

			const FVector Extent(SeparationRadius, SeparationRadius, 0.f);
			Neighbors.Reset();
			Grid.Query(FBox(SelfLocation - Extent, SelfLocation + Extent), Neighbors);

			FVector PushDirection = FVector::ZeroVector;
			int32 ConsideredCount = 0;

			for (const FMassNavigationObstacleItem& Neighbor : Neighbors)
			{
				if (Neighbor.Entity == SelfEntity)
				{
					continue;
				}

				if (ConsideredCount >= ChaseParams.MaxSeparationNeighbors)
				{
					break;
				}

				// 그리드는 핸들만 주므로 위치는 따로 조회한다.
				// 이웃이 방금 파괴됐을 수 있어 Ptr 계열로 받는다.
				const FTransformFragment* NeighborTransform =
					EntityManager.GetFragmentDataPtr<FTransformFragment>(Neighbor.Entity);
				if (NeighborTransform == nullptr)
				{
					continue;
				}

				// 수평 거리로만 민다. 높이차로 밀면 지면에서 튀어오른다.
				FVector Delta = SelfLocation - NeighborTransform->GetTransform().GetLocation();
				Delta.Z = 0.f;

				const float DistanceSq = Delta.SizeSquared();
				if (DistanceSq >= FMath::Square(SeparationRadius))
				{
					continue;
				}

				++ConsideredCount;

				if (DistanceSq <= KINDA_SMALL_NUMBER)
				{
					// 완전히 겹친 경우. 방향이 없으므로 핸들 값으로 결정론적인 방향을 만든다.
					// 난수를 쓰면 매 프레임 방향이 바뀌어 제자리에서 떨린다.
					const float Angle = (SelfEntity.Index % 360) * (UE_PI / 180.f);
					PushDirection += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
					continue;
				}

				const float Distance = FMath::Sqrt(DistanceSq);

				// 가까울수록 강하게. 경계에서 0이 되어야 힘이 튀지 않는다.
				const float Falloff = 1.f - (Distance / SeparationRadius);
				PushDirection += (Delta / Distance) * Falloff;
			}

			if (!PushDirection.IsNearlyZero())
			{
				ForceList[EntityIt].Value += PushDirection.GetSafeNormal() * ChaseParams.SeparationStrength;
			}
		}
	});
}

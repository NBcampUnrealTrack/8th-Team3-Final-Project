// TRMassSkinnedCustomDataProcessor.cpp

#include "Mass/Representation/TRMassSkinnedCustomDataProcessor.h"

#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassLODFragments.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationSubsystem.h"
#include "MassRepresentationTypes.h"
#include "MassSkinnedMeshRepresentationTypes.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Debug/TRMassDebug.h"

DECLARE_CYCLE_STAT(TEXT("Skinned Custom Data"), STAT_TRMassSkinnedCustomData, STATGROUP_TRMass);

UTRMassSkinnedCustomDataProcessor::UTRMassSkinnedCustomDataProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// 데디케이티드 서버는 인스턴스 메시를 그리지 않는다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;

	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;

	// 표현 타입(액터/ISM/없음)이 정해진 뒤라야 어느 엔티티가 인스턴스 메시인지 의미가 생긴다.
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::VisualizationProcessing);

	// 트랜스폼 프로세서는 PrevLODSignificance 를 자기가 훑으면서 덮어쓴다. 그 뒤에 읽으면
	// 이미 이번 프레임 값으로 바뀌어 있어, LOD 구간이 바뀌는 프레임에 커스텀 데이터가
	// 트랜스폼과 다른 구간으로 들어간다.
	ExecutionOrder.ExecuteBefore.Add(TEXT("MassUpdateInstancedSkinnedMeshProcessor"));

	// 표현 서브시스템의 공유 데이터를 만진다.
	bRequiresGameThreadExecution = true;
}

void UTRMassSkinnedCustomDataProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// ── 아래 구성은 UMassUpdateInstancedSkinnedMeshProcessor 와 같아야 한다 ──
	//
	// 그 프로세서가 인스턴스를 등록한 순서대로 float 을 덧붙여야 짝이 맞기 때문이다.
	// 엔진은 마지막에 커스텀 float 개수를 인스턴스 수로 나눠서 쓰므로, 하나라도
	// 어긋나면 전체가 한 칸씩 밀려 엉뚱한 적이 피격 플래시를 낸다.
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.SetChunkFilter(&FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);
	EntityQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassRepresentationSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassStaticRepresentationTag>(EMassFragmentPresence::None);

	// 여기부터가 우리 쪽 추가분. **Optional 이어야 한다** — 필수로 두면 이 프래그먼트가
	// 없는 엔티티가 위 프로세서에는 잡히고 여기서는 빠져 정렬이 깨진다.
	EntityQuery.AddRequirement<FTREnemyVisualFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UTRMassSkinnedCustomDataProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRMassSkinnedCustomData);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRMassSkinnedCustomData);

	const UWorld* World = EntityManager.GetWorld();
	const float WorldTime = World ? World->GetTimeSeconds() : 0.f;

	// 인스턴스마다 같은 길이를 보내야 하므로 배열을 한 번 만들어 값만 갈아 끼운다.
	// 청크 바깥에 두는 이유는 청크마다 새로 만들면 프레임마다 힙 할당이 청크 수만큼
	// 생기기 때문이다. 이 프로세서는 게임 스레드 전용이라 공유해도 안전하다.
	TArray<float> CustomFloats;
	CustomFloats.SetNumZeroed(NumCustomDataFloats);

	EntityQuery.ForEachEntityChunk(Context, [WorldTime, &CustomFloats](FMassExecutionContext& Context)
	{
		UMassRepresentationSubsystem* RepresentationSubsystem =
			Context.GetSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		if (RepresentationSubsystem == nullptr)
		{
			return;
		}

		FMassInstancedSkinnedMeshInfoArrayView SkinnedMeshInfos = RepresentationSubsystem->GetMutableInstancedSkinnedMeshInfos();

		const TConstArrayView<FMassRepresentationFragment> RepresentationList = Context.GetFragmentView<FMassRepresentationFragment>();
		const TConstArrayView<FMassRepresentationLODFragment> RepresentationLODList = Context.GetFragmentView<FMassRepresentationLODFragment>();
		const TConstArrayView<FTREnemyVisualFragment> VisualList = Context.GetFragmentView<FTREnemyVisualFragment>();

		const bool bHasVisual = (VisualList.Num() > 0);

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassRepresentationFragment& Representation = RepresentationList[EntityIt];

			// 액터로 그려지는 개체는 자기 메시에 직접 쓴다. 여기서 또 보내면 인스턴스
			// 목록에 없는 개체 몫이 끼어들어 정렬이 깨진다.
			if (Representation.CurrentRepresentation != EMassRepresentationType::SkinnedMeshInstance)
			{
				continue;
			}

			const int32 MeshInfoIndex = Representation.SkinnedMeshDescHandle.ToIndex();
			if (!SkinnedMeshInfos.IsValidIndex(MeshInfoIndex))
			{
				continue;
			}

			// 손상 프래그먼트가 없는 개체도 0을 보낸다. 건너뛰면 개수가 어긋난다.
			if (bHasVisual)
			{
				const FTREnemyVisualFragment& Visual = VisualList[EntityIt];
				CustomFloats[DamageCustomDataIndex] = Visual.DamageRatio;
				CustomFloats[HitFlashCustomDataIndex] = UE::TR::Enemy::ComputeHitFlash(Visual, WorldTime);
			}
			else
			{
				CustomFloats[DamageCustomDataIndex] = 0.f;
				CustomFloats[HitFlashCustomDataIndex] = 0.f;
			}

			SkinnedMeshInfos[MeshInfoIndex].AddBatchedCustomDataFloats(
				CustomFloats,
				RepresentationLODList[EntityIt].LODSignificance,
				Representation.PrevLODSignificance);
		}
	});
}

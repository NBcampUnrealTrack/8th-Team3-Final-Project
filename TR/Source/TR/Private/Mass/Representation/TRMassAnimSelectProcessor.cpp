// TRMassAnimSelectProcessor.cpp

#include "Mass/Representation/TRMassAnimSelectProcessor.h"

#include "Animation/AnimSequenceTransformProviderData.h"
#include "Engine/World.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassRepresentationAnimationProcessor.h"
#include "MassRepresentationFragments.h"
// UE::Mass::ProcessorGroupNames::Representation / VisualizationProcessing이 여기 있다.
// (공용 MassCommonTypes.h가 아니라 표현 모듈이 자기 네임스페이스에 덧붙이는 이름들이다.)
#include "MassRepresentationTypes.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Core/TRMassParams.h"
#include "Mass/Debug/TRMassDebug.h"

DECLARE_CYCLE_STAT(TEXT("Anim Select"), STAT_TRMassAnimSelect, STATGROUP_TRMass);

UTRMassAnimSelectProcessor::UTRMassAnimSelectProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// 데디케이티드 서버는 인스턴스 메시를 그리지 않는다. 소비 프로세서와 조건을 맞춘다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;

	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;

	// 표현 타입(액터/ISM/없음)이 정해진 뒤라야 어느 엔티티가 ISM인지 의미가 생긴다.
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::VisualizationProcessing);

	// 같은 프레임에 반영되려면 소비 프로세서보다 앞이어야 한다. 뒤로 밀리면 늘 1프레임 늦는다.
	ExecutionOrder.ExecuteBefore.Add(UMassConsumeInstancedSkinnedMeshAnimationProcessor::StaticClass()->GetFName());

	// 프래그먼트만 만진다. 게임 스레드에 묶을 이유가 없다.
	bRequiresGameThreadExecution = false;
}

void UTRMassAnimSelectProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FMassRepresentationAnimationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTREnemyStateFragment>(EMassFragmentAccess::ReadOnly);

	// 이동 트레이트가 없는 설정(고정 포탑 등)도 대기 애니메이션은 나와야 하므로 선택 사항이다.
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

	EntityQuery.AddConstSharedRequirement<FTREnemyAnimParams>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);
}

void UTRMassAnimSelectProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRMassAnimSelect);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRMassAnimSelect);

	const UWorld* World = EntityManager.GetWorld();
	const float WorldTime = World ? World->GetTimeSeconds() : 0.f;

	EntityQuery.ForEachEntityChunk(Context, [WorldTime](FMassExecutionContext& Context)
	{
		const FTREnemyAnimParams& Params = Context.GetConstSharedFragment<FTREnemyAnimParams>();

		const TArrayView<FMassRepresentationAnimationFragment> AnimList = Context.GetMutableFragmentView<FMassRepresentationAnimationFragment>();
		const TConstArrayView<FTREnemyStateFragment> StateList = Context.GetFragmentView<FTREnemyStateFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();

		const bool bHasVelocity = (VelocityList.Num() > 0);

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTREnemyStateFragment& State = StateList[EntityIt];
			const float StateElapsed = WorldTime - State.StateEnteredTime;
			const float Speed = bHasVelocity ? VelocityList[EntityIt].Value.Size2D() : 0.f;

			FAnimSequenceTrackAutoPlayData& AnimData = AnimList[EntityIt].AnimData;

			// 직전 프레임의 선택을 그대로 상태로 쓴다. 히스테리시스 하나 때문에 프래그먼트를
			// 늘리지 않아도 되고, 표현이 실제로 재생 중인 것과 판단 근거가 항상 일치한다.
			const bool bWasMoving = (AnimData.SequenceIndex == Params.Move.SequenceIndex);

			const UE::TR::Enemy::FAnimSelection Selection =
				UE::TR::Enemy::SelectAnim(Params, State.ActionState, StateElapsed, Speed, bWasMoving);

			AnimData.SequenceIndex = Selection.Sequence.SequenceIndex;
			AnimData.PlayRate = FMath::Max(Selection.Sequence.PlayRate, KINDA_SMALL_NUMBER);
			AnimData.BlendTime = Params.BlendTime;
			AnimData.LoopMode = Selection.bLooping
				? EAnimSequenceTrackLoopMode::Loop
				: EAnimSequenceTrackLoopMode::Clamp;

			// 시작 위치. 단발은 항상 처음부터, 루프는 개체마다 다른 지점에서 시작해
			// 무리 전체가 같은 발을 딛는 걸 막는다.
			//
			// 엔티티 핸들로 위상을 뽑는 이유는 프레임과 무관하게 같은 값이 나와야 하기
			// 때문이다. 난수를 매 프레임 뽑으면 인덱스가 바뀔 때마다 재생 위치가 튄다.
			float StartPosition = 0.f;
			if (Selection.bLooping && Params.bDesyncLoopPhase && Selection.Sequence.Duration > 0.f)
			{
				const uint32 Hash = GetTypeHash(EntityIt.GetEntityHandle());
				const float Phase = static_cast<float>(Hash & 0xFFFF) / 65535.f;

				StartPosition = Phase * Selection.Sequence.Duration;
			}
			AnimData.Position = StartPosition;
		}
	});
}

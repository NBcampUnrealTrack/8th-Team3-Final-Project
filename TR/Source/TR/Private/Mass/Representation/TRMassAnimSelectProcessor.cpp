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
#include "Environment/TRGroundMotionSubsystem.h"
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

	// 지면이 스크롤하므로 이동/대기 판정에 월드 속력을 쓸 수 없다.
	EntityQuery.AddSubsystemRequirement<UTRGroundMotionSubsystem>(EMassFragmentAccess::ReadOnly);
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
		const FVector GroundVelocity =
			Context.GetSubsystemChecked<UTRGroundMotionSubsystem>().GetGroundVelocity();

		const TArrayView<FMassRepresentationAnimationFragment> AnimList = Context.GetMutableFragmentView<FMassRepresentationAnimationFragment>();
		const TConstArrayView<FTREnemyStateFragment> StateList = Context.GetFragmentView<FTREnemyStateFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();

		const bool bHasVelocity = (VelocityList.Num() > 0);

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTREnemyStateFragment& State = StateList[EntityIt];
			FAnimSequenceTrackAutoPlayData& AnimData = AnimList[EntityIt].AnimData;

			// 직전 프레임의 선택을 그대로 상태로 쓴다. 히스테리시스 때문에 프래그먼트를
			// 늘리지 않아도 되고, 판단 근거가 실제 재생 중인 것과 항상 일치한다.
			//
			// 다만 단발 재생 중에는 인덱스가 공격 클립이라 여기서 false가 된다. 액터 표현은
			// 로코모션 판정을 계속 추적하므로(FAnimResolution::bMoving) 단발이 끝나는 순간
			// 두 경로의 임계값이 갈린다. 지면이 흐르는 동안에는 속력이 임계값을 한참 넘어
			// 닿지 않지만, 기차가 멈추는 연출이 생기면 여기에 상태를 하나 둬야 한다.
			const bool bWasMoving = (AnimData.SequenceIndex == Params.Move.SequenceIndex);

			UE::TR::Enemy::FAnimContext AnimContext;
			AnimContext.ActionState = State.ActionState;
			AnimContext.StateElapsed = WorldTime - State.StateEnteredTime;
			AnimContext.Velocity = bHasVelocity ? VelocityList[EntityIt].Value : FVector::ZeroVector;
			AnimContext.GroundVelocity = GroundVelocity;
			AnimContext.bWasMoving = bWasMoving;
			AnimContext.EntityHash = GetTypeHash(EntityIt.GetEntityHandle());
			AnimContext.WorldTime = WorldTime;

			const UE::TR::Enemy::FAnimResolution Anim = UE::TR::Enemy::ResolveAnim(Params, AnimContext);

			AnimData.SequenceIndex = Anim.Sequence.SequenceIndex;
			AnimData.PlayRate = FMath::Max(Anim.Sequence.PlayRate, KINDA_SMALL_NUMBER);
			AnimData.BlendTime = Params.BlendTime;
			AnimData.LoopMode = Anim.bLooping
				? EAnimSequenceTrackLoopMode::Loop
				: EAnimSequenceTrackLoopMode::Clamp;
			AnimData.Position = Anim.GetPosition();
		}
	});
}

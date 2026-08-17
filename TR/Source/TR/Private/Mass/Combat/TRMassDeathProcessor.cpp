// TRMassDeathProcessor.cpp

#include "Mass/Combat/TRMassDeathProcessor.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "Movement/MassMovementProcessors.h"
#include "Environment/TRGroundMotionSubsystem.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Core/TRMassParams.h"
#include "Mass/Debug/TRMassDebug.h"

DECLARE_CYCLE_STAT(TEXT("Enemy Death"), STAT_TRMassDeath, STATGROUP_TRMass);

UTRMassDeathProcessor::UTRMassDeathProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// 엔티티 삭제는 서버 권위. 클라이언트는 복제된 표현만 본다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;

	// **엔진이 속도를 트랜스폼에 적용하기 직전**에 돈다.
	//
	// 앞쪽(Behavior)에 두면 뒤따르는 조종·힘 적용이 값을 다시 채워 넣을 수 있고, 실제로
	// 시체가 계속 목표를 향해 나아갔다. 무엇이 속도를 쓰든 마지막에 여기서 끊으면
	// 그 프레임의 이동은 0이 된다 — 쓰는 쪽을 하나씩 찾아 막는 것보다 확실하다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::ApplyForces);
	ExecutionOrder.ExecuteBefore.Add(UMassApplyMovementProcessor::StaticClass()->GetFName());

	// 디버그 드로우가 게임 스레드 전용이다. 시체만 도는 프로세서라 비용은 무시할 만하다.
	bRequiresGameThreadExecution = true;
}

void UTRMassDeathProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTREnemyStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FTREnemyCombatParams>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FTRDeadTag>(EMassFragmentPresence::All);

	// 시체를 매 프레임 정지시킨다.
	//
	// **DesiredVelocity 가 진짜 상태다.** UMassApplyMovementProcessor 는 매 프레임
	// `Velocity = DesiredVelocity` 로 덮어쓴 뒤 그 값으로 트랜스폼을 옮긴다. 그래서
	// FMassVelocityFragment 만 지우면 다음 프레임에 그대로 되살아난다.
	//
	// 힘은 지울 필요가 없다 — UMassApplyForceProcessor 가 적용 직후 스스로 0으로 되돌린다.
	// 그래도 함께 끊어 두는 편이 이 프레임의 조종이 남기는 잔여를 확실히 없앤다.
	EntityQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);

	// 시체는 지면과 함께 흘러가야 한다. 스스로 달리는 산 개체와 정반대다.
	EntityQuery.AddSubsystemRequirement<UTRGroundMotionSubsystem>(EMassFragmentAccess::ReadOnly);
}

void UTRMassDeathProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRMassDeath);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRMassDeath);

	const UWorld* World = EntityManager.GetWorld();
	const float WorldTime = World ? World->GetTimeSeconds() : 0.f;

	// 시체가 계속 움직이는 문제를 눈으로 가르기 위한 표시.
	// 구가 보이면 이 프로세서가 그 개체를 잡고 있다는 뜻이고, 옆의 숫자가 0인데도
	// 시체가 움직인다면 속도가 아닌 다른 무언가가 트랜스폼을 옮기는 것이다.
	const bool bDrawDebug = UE::TR::MassDebug::IsDrawEnabled();
	UWorld* DebugWorld = const_cast<UWorld*>(World);

	EntityQuery.ForEachEntityChunk(Context, [WorldTime, bDrawDebug, DebugWorld](FMassExecutionContext& Context)
	{
		const FTREnemyCombatParams& Params = Context.GetConstSharedFragment<FTREnemyCombatParams>();

		// 시체가 따라갈 속도. 지면이 멈춘 레벨에서는 0이라 그 자리에 선다.
		const FVector GroundVelocity =
			Context.GetSubsystemChecked<UTRGroundMotionSubsystem>().GetGroundVelocity();
		const TConstArrayView<FTREnemyStateFragment> StateList = Context.GetFragmentView<FTREnemyStateFragment>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

		const TArrayView<FMassDesiredMovementFragment> DesiredList = Context.GetMutableFragmentView<FMassDesiredMovementFragment>();
		const TArrayView<FMassVelocityFragment> VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();
		const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();

		const bool bHasDesired = (DesiredList.Num() > 0);
		const bool bHasVelocity = (VelocityList.Num() > 0);
		const bool bHasForce = (ForceList.Num() > 0);

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
#if !UE_BUILD_SHIPPING
			if (bDrawDebug && DebugWorld != nullptr)
			{
				const FVector Location = TransformList[EntityIt].GetTransform().GetLocation();

				// 지우기 **전**의 값을 찍는다. 지운 뒤를 찍으면 늘 0이라 아무것도 알 수 없다.
				DrawDebugSphere(DebugWorld, Location, 60.f, 8, FColor::Magenta, false, 0.f, 0, 2.f);
				// desired 가 실제로 이동을 결정하는 값이다. v 는 그 복사본일 뿐이라
				// 둘을 같이 봐야 어디서 되살아나는지 알 수 있다.
				DrawDebugString(DebugWorld, Location + FVector(0.f, 0.f, 120.f),
					FString::Printf(TEXT("DEAD desired=%.0f v=%.0f"),
						bHasDesired ? DesiredList[EntityIt].DesiredVelocity.Size() : -1.f,
						bHasVelocity ? VelocityList[EntityIt].Value.Size() : -1.f),
					nullptr, FColor::Magenta, 0.f);
			}
#endif

			// 매 프레임 지면 속도로 덮는다.
			//
			// 0으로 두면 시체가 기차와 같은 거리를 유지한 채 발밑 지면만 흘러간다.
			// 스스로 달리는 산 개체와 달리 시체는 지면에 얹힌 물체이므로 함께 밀려나야 하고,
			// 그래야 연출이 끝나기 전에도 자연스럽게 뒤로 멀어진다.
			//
			// 죽기 직전의 DesiredVelocity 는 아무도 지워주지 않으므로 매 프레임 덮어야 한다.
			if (bHasDesired)
			{
				DesiredList[EntityIt].DesiredVelocity = GroundVelocity;
			}
			if (bHasVelocity)
			{
				VelocityList[EntityIt].Value = GroundVelocity;
			}
			if (bHasForce)
			{
				ForceList[EntityIt].Value = FVector::ZeroVector;
			}

			// 진입 시각은 죽은 순간이다. 표현이 사망 클립의 재생 위치를 재는 것과 같은 원점이라,
			// 연출이 끝나는 시점과 시체가 사라지는 시점이 저절로 맞는다.
			const float Elapsed = WorldTime - StateList[EntityIt].StateEnteredTime;
			if (Elapsed < Params.DeathLinger)
			{
				continue;
			}

			Context.Defer().DestroyEntity(EntityIt.GetEntityHandle());
		}
	});
}

// TRChaseSteeringProcessor.cpp

#include "Mass/Movement/TRChaseSteeringProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassNavigationUtils.h"
#include "MassSignalSubsystem.h"
#include "MassSimulationLOD.h"
#include "MassStateTreeTypes.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Core/TRMassParams.h"

DECLARE_CYCLE_STAT(TEXT("Chase Steering"), STAT_TRChaseSteering, STATGROUP_TRMass);

namespace
{
	/**
	 * 회전 기준을 "이동 중"과 "정지"로 섞는 비율. 1이면 이동, 0이면 정지 기준.
	 *
	 * 목표에 가까워질수록 0으로 간다. 제곱을 씌워 전환이 도착 직전에 몰리게 한다
	 * (엔진 UMassSmoothOrientationProcessor와 같은 방식). 선형으로 두면 멀리서부터
	 * 목표를 빤히 쳐다보며 걸어와 부자연스럽다.
	 */
	float ComputeMovingBlend(float DistanceToTarget, float FadeDistance)
	{
		if (FadeDistance <= KINDA_SMALL_NUMBER)
		{
			return 1.f;
		}

		return FMath::Square(FMath::Clamp(DistanceToTarget / FadeDistance, 0.f, 1.f));
	}

	/**
	 * 목표 방향과 속도 방향을 섞어 부드럽게 회전시킨다.
	 *
	 * 엔진 이동 프로세서는 회전을 건드리지 않으므로 여기서 처리한다. 기법은
	 * UMassSmoothOrientationProcessor와 동일하지만, 그 트레이트는 FMassMoveTargetFragment를
	 * 요구해서 우리 설계와 맞지 않아 직접 구현했다.
	 *
	 * @param MovingBlend 1이면 이동 중 가중치, 0이면 정지 가중치. 그 사이는 보간.
	 */
	void ApplySmoothOrientation(FTransform& Transform, const FTRChaseParams& ChaseParams,
		const FVector& CurrentVelocity, const FVector& TargetDirection, float MovingBlend, float DeltaTime)
	{
		const FVector CurrentForward = Transform.GetRotation().GetForwardVector();

		// 속도가 거의 0이면 방향이 없다. 그대로 쓰면 Atan2(0,0)=0이라 +X로 홱 돈다.
		const FVector VelocityDirection = CurrentVelocity.IsNearlyZero() ? CurrentForward : CurrentVelocity;

		const FVector::FReal VelocityHeading = UE::MassNavigation::GetYawFromDirection(VelocityDirection);
		const FVector::FReal TargetHeading = TargetDirection.IsZero()
			? UE::MassNavigation::GetYawFromDirection(CurrentForward)
			: UE::MassNavigation::GetYawFromDirection(TargetDirection);

		const float TargetWeight = FMath::Lerp(ChaseParams.StandingTargetWeight, ChaseParams.MovingTargetWeight, MovingBlend);
		const float VelocityWeight = FMath::Lerp(ChaseParams.StandingVelocityWeight, ChaseParams.MovingVelocityWeight, MovingBlend);

		const float WeightSum = TargetWeight + VelocityWeight;
		if (WeightSum <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const FVector::FReal Ratio = TargetWeight / WeightSum;
		const FVector::FReal DesiredHeading = UE::MassNavigation::LerpAngle(VelocityHeading, TargetHeading, Ratio);

		const FVector::FReal CurrentHeading = UE::MassNavigation::GetYawFromDirection(CurrentForward);
		const FVector::FReal NewHeading = UE::MassNavigation::ExponentialSmoothingAngle(
			CurrentHeading, DesiredHeading, DeltaTime, ChaseParams.OrientationSmoothingTime);

		Transform.SetRotation(FQuat(FVector::UpVector, NewHeading));
	}
}

UTRChaseSteeringProcessor::UTRChaseSteeringProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;

	// 조종력은 Avoidance 그룹에서 만든다. 엔진의 UMassApplyForceProcessor가
	// ApplyForces 그룹에서 이 그룹 뒤에 돌면서 힘을 속도로 적분한다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Behavior);
}

void UTRChaseSteeringProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTRTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);

	// 정지 거리와 속도는 개체마다 다르다(종류·사출 여부). TR Chaser 트레이트가 항상 붙인다.
	EntityQuery.AddRequirement<FTRApproachFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddConstSharedRequirement<FTRChaseParams>(EMassFragmentPresence::All);
	// 최고 속도는 MassMovement가 관리한다. 여기서 중복 정의하지 않고 그 값을 그대로 쓴다.
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);

	// 시뮬레이션 LOD가 붙어 있으면 원거리 청크는 낮은 빈도로 돈다.
	// 건너뛴 프레임만큼 DeltaTime이 누적되므로 이동 거리는 보존된다.
	EntityQuery.AddRequirement<FMassSimulationVariableTickFragment>(
		EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddChunkRequirement<FMassSimulationVariableTickChunkFragment>(
		EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.SetChunkFilter(&FMassSimulationVariableTickChunkFragment::ShouldTickChunkThisFrame);

	// 도달/이탈 전이를 StateTree에 알리기 위해 필요하다.
	// Mass StateTree는 신호로만 깨어나므로, 판정을 하는 쪽이 신호를 보내야 한다.
	//
	// 반드시 ProcessorRequirements에 선언한다. EntityQuery 쪽에 붙이면
	// 쿼리가 엔티티를 매칭하지 못해 프로세서가 통째로 돌지 않는다.
	// 신호 발송은 개체별 데이터가 아니라 프로세서 수준의 작업이기 때문이다.
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UTRChaseSteeringProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRChaseSteering);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRChaseSteering);

	const float WorldDeltaTime = Context.GetDeltaTimeSeconds();

	// 신호 대상은 청크를 넘어 모았다가 마지막에 한 번에 보낸다.
	// 전이는 드물게 일어나므로 대개 비어 있고, 인라인 버퍼로 힙 할당을 피한다.
	TArray<FMassEntityHandle, TInlineAllocator<32>> EntitiesToSignal;

	EntityQuery.ForEachEntityChunk(Context, [WorldDeltaTime, &EntitiesToSignal](FMassExecutionContext& Context)
	{
		const TConstArrayView<FMassSimulationVariableTickFragment> VariableTickList =
			Context.GetFragmentView<FMassSimulationVariableTickFragment>();
		const bool bHasVariableTick = (VariableTickList.Num() > 0);

		const FTRChaseParams& ChaseParams = Context.GetConstSharedFragment<FTRChaseParams>();
		const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();

		const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FTRTargetFragment> TargetList = Context.GetMutableFragmentView<FTRTargetFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();
		const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();

		const TConstArrayView<FTRApproachFragment> ApproachList = Context.GetFragmentView<FTRApproachFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FTRTargetFragment& Target = TargetList[EntityIt];
			FTransform& Transform = TransformList[EntityIt].GetMutableTransform();
			const FVector CurrentVelocity = VelocityList[EntityIt].Value;
			const FTRApproachFragment& Approach = ApproachList[EntityIt];

			// 속도는 보통 Mass Movement가 정하고, 사출형처럼 단계마다 달라지는 개체만
			// 자기 값을 갖는다. 명시된 값에는 비율을 곱하지 않는다 — 이미 "얼마로 달릴지"가
			// 정해진 수치라 또 곱하면 기획 수치(상체 25 m/s)와 어긋난다.
			//
			// 어느 쪽이든 엔진의 UMassApplyForceProcessor가 마지막에
			// FMassMovementParameters::MaxSpeed로 클램프한다. 그 값이 이 엔티티가 낼 수 있는
			// 천장이므로, 단계 속도를 쓰는 Config는 Max Speed를 그중 최댓값으로 잡아야 한다.
			const float DesiredSpeed = (Approach.MaxSpeed > 0.f)
				? Approach.MaxSpeed
				: MovementParams.MaxSpeed * ChaseParams.SpeedRatio;

			// 도착 몇 초 전부터 회전 기준을 바꿀지를 거리로 환산해 둔다.
			const float OrientationFadeDistance = ChaseParams.OrientationFadeTime * DesiredSpeed;

			// 큰 시간 간격에서 힘이 폭발하는 걸 막는다(엔진 이동 프로세서와 동일한 상한).
			const float DeltaTime = FMath::Min(0.1f,
				bHasVariableTick ? VariableTickList[EntityIt].DeltaTime : WorldDeltaTime);

			FVector DesiredVelocity = FVector::ZeroVector;

			// 회전 기준이 될 목표 방향. 목표가 없으면 현재 방향을 유지한다.
			FVector TargetDirection = FVector::ZeroVector;
			float DistanceToTarget = 0.f;
			bool bStanding = true;

			if (Target.bHasTarget)
			{
				// 수평 거리로만 판정한다. 지형 높이차 때문에 도달 판정이 빗나가는 걸 막는다.
				FVector ToTarget = Target.TargetLocation - Transform.GetLocation();
				ToTarget.Z = 0.f;
				const float Distance = ToTarget.Size();

				// 도달/이탈 판정에 서로 다른 반경을 써서 경계에서 태그가 떨리지 않게 한다(히스테리시스).
				const float Threshold = Target.bArrived ? Approach.LeaveDistance : Approach.StopDistance;
				const bool bIsArrived = (Distance <= Threshold);

				if (bIsArrived != Target.bArrived)
				{
					// 상태가 바뀐 순간에만 아키타입을 옮긴다. 매 틱 명령을 쌓으면 낭비다.
					Target.bArrived = bIsArrived;
					if (bIsArrived)
					{
						Context.Defer().AddTag<FTRArrivedTag>(EntityIt.GetEntityHandle());
					}
					else
					{
						Context.Defer().RemoveTag<FTRArrivedTag>(EntityIt.GetEntityHandle());
					}

					// 행동 상태(FTREnemyStateFragment)는 여기서 건드리지 않는다.
					// StateTree Task가 그 값의 단독 주인이다. 두 곳에서 쓰면 어느 쪽이
					// 이겼는지에 따라 애니메이션이 달라져 재현이 안 되는 버그가 된다.
					//
					// 대신 "상태가 바뀌었으니 다시 판단하라"고 알린다. Mass StateTree는
					// 매 틱 평가되지 않고 신호로만 깨어나므로, 이 알림이 없으면 트리가
					// 도달 사실을 영원히 모른다.
					EntitiesToSignal.Add(EntityIt.GetEntityHandle());
				}

				if (Distance > KINDA_SMALL_NUMBER)
				{
					TargetDirection = ToTarget / Distance;
					DistanceToTarget = Distance;
				}

				bStanding = bIsArrived;

				if (!bIsArrived && !TargetDirection.IsZero())
				{
					DesiredVelocity = TargetDirection * DesiredSpeed;
				}
			}
			else if (Target.bArrived)
			{
				// 목표가 사라졌는데 도달 상태만 남은 경우. 도달은 "내 목표에 닿았다"는
				// 뜻이라 목표가 없으면 성립하지 않는다. 여기서 풀지 않으면 태그가 참인 채
				// 굳어서, 공격 프로세서가 없는 대상을 계속 때린다.
				Target.bArrived = false;
				Context.Defer().RemoveTag<FTRArrivedTag>(EntityIt.GetEntityHandle());
				EntitiesToSignal.Add(EntityIt.GetEntityHandle());
			}

			// 조종력 = 원하는 속도와 현재 속도의 차이. 엔진이 MaxAcceleration으로 잘라준다.
			// 분리력은 별도 프로세서가 같은 프래그먼트에 누적한다.
			ForceList[EntityIt].Value += (DesiredVelocity - CurrentVelocity);

			// 이동 중일수록 1에 가깝다. 멈췄거나 목표가 없으면 0(= 정지 기준으로 회전).
			const float MovingBlend = (bStanding || !Target.bHasTarget)
				? 0.f : ComputeMovingBlend(DistanceToTarget, OrientationFadeDistance);

			ApplySmoothOrientation(Transform, ChaseParams, CurrentVelocity, TargetDirection,
				MovingBlend, DeltaTime);
		}
	});

	if (!EntitiesToSignal.IsEmpty())
	{
		UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
		SignalSubsystem.SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, EntitiesToSignal);
	}
}

// TREnemyAttackProcessor.cpp

#include "Mass/Combat/TREnemyAttackProcessor.h"

#include "Engine/World.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "Mass/Combat/TRMassCombatSubsystem.h"
#include "Mass/Core/TRMassParams.h"
#include "Mass/Debug/TRMassDebug.h"

DECLARE_CYCLE_STAT(TEXT("Enemy Attack"), STAT_TREnemyAttack, STATGROUP_TRMass);

namespace
{
	/** 피해 한 건을 게임 스레드 큐로 보낸다. 자폭·상체 충돌·사격이 모두 이 경로를 쓴다. */
	void EmitImpact(UTRMassCombatSubsystem& Combat, const FTransformFragment& Transform,
		float Damage, ETREnemyType SourceType, bool bExplosion, ETRTargetChannel Channel)
	{
		FTRMassImpactEvent Impact;
		Impact.Location = Transform.GetTransform().GetLocation();
		Impact.Damage = Damage;
		Impact.SourceType = SourceType;
		Impact.bExplosion = bExplosion;

		// 피해를 받을 액터는 이 채널의 목표에서 찾는다. 적이 쫓던 대상과
		// 피해를 받는 대상이 항상 같도록, 판단을 여기서 하지 않고 채널만 넘긴다.
		Impact.Channel = Channel;

		Combat.EnqueueImpact(Impact);
	}
}

UTREnemyAttackProcessor::UTREnemyAttackProcessor()
	: ArrivalQuery(*this)
	, DetonationQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// 피해 판정은 서버 권위. 클라이언트는 복제된 표현만 본다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;

	// 행동 판단이므로 Behavior 그룹. 조종력을 만드는 Avoidance 그룹보다 앞이다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
}

void UTREnemyAttackProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// ── 도달 → 행동 시작 ──────────────────────────────────────────────────
	ArrivalQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ArrivalQuery.AddRequirement<FTREnemyCombatFragment>(EMassFragmentAccess::ReadWrite);
	ArrivalQuery.AddRequirement<FTREnemyStateFragment>(EMassFragmentAccess::ReadWrite);
	ArrivalQuery.AddRequirement<FTRApproachFragment>(EMassFragmentAccess::ReadWrite);
	// 피해를 받을 액터를 고르는 근거. 판단은 하지 않고 채널만 실어 보낸다.
	ArrivalQuery.AddRequirement<FTRChaseChannelFragment>(EMassFragmentAccess::ReadOnly);
	// 사출로 정지 거리를 바꿀 때 도달 상태를 무효화해야 한다(아래 Launcher 분기 참고).
	ArrivalQuery.AddRequirement<FTRTargetFragment>(EMassFragmentAccess::ReadWrite);
	ArrivalQuery.AddConstSharedRequirement<FTREnemyCombatParams>(EMassFragmentPresence::All);
	ArrivalQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);
	ArrivalQuery.AddTagRequirement<FTRArrivedTag>(EMassFragmentPresence::All);
	ArrivalQuery.AddTagRequirement<FTRDetonatingTag>(EMassFragmentPresence::None);
	ArrivalQuery.AddSubsystemRequirement<UTRMassCombatSubsystem>(EMassFragmentAccess::ReadWrite);

	// ── 폭발 예약 → 실행 ──────────────────────────────────────────────────
	DetonationQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	DetonationQuery.AddRequirement<FTREnemyCombatFragment>(EMassFragmentAccess::ReadOnly);
	DetonationQuery.AddRequirement<FTRChaseChannelFragment>(EMassFragmentAccess::ReadOnly);
	DetonationQuery.AddConstSharedRequirement<FTREnemyCombatParams>(EMassFragmentPresence::All);
	DetonationQuery.AddTagRequirement<FTRDetonatingTag>(EMassFragmentPresence::All);
	DetonationQuery.AddSubsystemRequirement<UTRMassCombatSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UTREnemyAttackProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TREnemyAttack);
	TRACE_CPUPROFILER_EVENT_SCOPE(TREnemyAttack);

	const UWorld* World = Context.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float WorldTime = World->GetTimeSeconds();

	// ── 1. 도달한 적의 행동 개시 ──────────────────────────────────────────
	ArrivalQuery.ForEachEntityChunk(Context, [WorldTime](FMassExecutionContext& Context)
	{
		const FTREnemyCombatParams& Params = Context.GetConstSharedFragment<FTREnemyCombatParams>();
		UTRMassCombatSubsystem& Combat = Context.GetMutableSubsystemChecked<UTRMassCombatSubsystem>();

		// 이미 사출된 상체는 종류가 사출형이어도 행동은 근접형과 같다(접촉하면 터진다).
		// 태그는 아키타입 단위라 청크당 한 번만 확인하면 된다.
		const ETREnemyType Behavior = Context.DoesArchetypeHaveTag<FTRLaunchedTag>()
			? ETREnemyType::Melee : Params.EnemyType;

		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FTREnemyCombatFragment> CombatList = Context.GetMutableFragmentView<FTREnemyCombatFragment>();
		const TArrayView<FTREnemyStateFragment> StateList = Context.GetMutableFragmentView<FTREnemyStateFragment>();
		const TArrayView<FTRApproachFragment> ApproachList = Context.GetMutableFragmentView<FTRApproachFragment>();
		const TConstArrayView<FTRChaseChannelFragment> ChannelList = Context.GetFragmentView<FTRChaseChannelFragment>();
		const TArrayView<FTRTargetFragment> TargetList = Context.GetMutableFragmentView<FTRTargetFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = EntityIt.GetEntityHandle();
			FTREnemyCombatFragment& Combatant = CombatList[EntityIt];
			FTREnemyStateFragment& State = StateList[EntityIt];

			switch (Behavior)
			{
			case ETREnemyType::Launcher:
			{
				// 상체를 사출한다. 새 엔티티를 만들지 않고 이 엔티티의 성격을 바꾼다.
				// 하체는 그 자리에 버려진 뒤 즉시 소멸하는데(기획서), 그건 연출이므로
				// 표현 쪽(액터/이펙트)이 ActionState == Launch를 보고 처리한다.
				UE::TR::Enemy::ApplyLaunched(Params, Combatant, ApproachList[EntityIt]);
				EnterActionState(State, ETREnemyActionState::Launch, WorldTime);

				// 도달 상태를 여기서 직접 무효화한다.
				//
				// 도달 판정의 주인은 Steering이지만, **사출은 자기 정지 거리를 스스로
				// 바꾸는 행동**이라 옛 거리(40m)로 내린 판정은 이 순간 의미를 잃는다.
				// 무효화까지가 사출이라는 행동의 일부다.
				//
				// Steering에 맡기면 안 되는 이유: Steering은 시뮬레이션 LOD 청크 필터를
				// 쓰기 때문에 프레임을 건너뛸 수 있다. 이 프로세서는 매 프레임 도므로,
				// 건너뛴 프레임에는 도달 태그가 남은 채로 다시 여기 잡히고 —
				// 그때는 FTRLaunchedTag 때문에 근접형으로 분기해서 40m 지점에서 터진다.
				TargetList[EntityIt].bArrived = false;
				Context.Defer().RemoveTag<FTRArrivedTag>(Entity);

				Context.Defer().AddTag<FTRLaunchedTag>(Entity);
				break;
			}

			case ETREnemyType::Shooter:
			{
				// 유지 거리에서 나란히 달리며 사격. 첫 발은 도달 즉시 나간다.
				if (Combatant.NextActionTime > 0.f && WorldTime < Combatant.NextActionTime)
				{
					break;
				}

				EmitImpact(Combat, TransformList[EntityIt], Combatant.ShotDamage,
					ETREnemyType::Shooter, /*bExplosion=*/false, ChannelList[EntityIt].Channel);

				++Combatant.ShotsFired;
				Combatant.NextActionTime = WorldTime + Params.FireInterval;
				EnterActionState(State, ETREnemyActionState::Fire, WorldTime);

				if (Combatant.ShotsFired >= Params.ShotCount)
				{
					// 탄을 소진하면 자체 폭발로 사라진다. 기차에 피해도, 자원도 없다.
					EnterActionState(State, ETREnemyActionState::Death, WorldTime);
					Context.Defer().DestroyEntity(Entity);
				}
				break;
			}

			case ETREnemyType::Melee:
			default:
			{
				// 접촉. 준비 시간을 재고 폭발 예약 상태로 넘긴다.
				Combatant.NextActionTime = WorldTime + Params.DetonationWindup;
				EnterActionState(State, ETREnemyActionState::Attack, WorldTime);

				Context.Defer().AddTag<FTRDetonatingTag>(Entity);
				break;
			}
			}
		}
	});

	// ── 2. 준비가 끝난 폭발 실행 ──────────────────────────────────────────
	DetonationQuery.ForEachEntityChunk(Context, [WorldTime](FMassExecutionContext& Context)
	{
		const FTREnemyCombatParams& Params = Context.GetConstSharedFragment<FTREnemyCombatParams>();
		UTRMassCombatSubsystem& Combat = Context.GetMutableSubsystemChecked<UTRMassCombatSubsystem>();

		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTREnemyCombatFragment> CombatList = Context.GetFragmentView<FTREnemyCombatFragment>();
		const TConstArrayView<FTRChaseChannelFragment> ChannelList = Context.GetFragmentView<FTRChaseChannelFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTREnemyCombatFragment& Combatant = CombatList[EntityIt];
			if (WorldTime < Combatant.NextActionTime)
			{
				continue;
			}

			EmitImpact(Combat, TransformList[EntityIt], Combatant.ContactDamage,
				Params.EnemyType, /*bExplosion=*/true, ChannelList[EntityIt].Channel);

			// 자폭한 적은 자원을 주지 않는다(기획서). 그래서 여기서 자원 처리를 하지 않고
			// 그냥 지운다. 자원은 "처치"된 경우에만 나가야 한다.
			Context.Defer().DestroyEntity(EntityIt.GetEntityHandle());
		}
	});
}

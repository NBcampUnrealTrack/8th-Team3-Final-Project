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
#include "Mass/Net/TRMassNetContract.h"
#include "Mass/Debug/TRMassDebug.h"

DECLARE_CYCLE_STAT(TEXT("Enemy Attack"), STAT_TREnemyAttack, STATGROUP_TRMass);

namespace
{
	/**
	 * 피해 한 건을 게임 스레드 큐로 보낸다. 자폭·상체 충돌·사격이 모두 이 경로를 쓴다.
	 * Location 은 기차 표면의 착탄점이다 — 받는 쪽이 이 점으로 장갑을 찾는다.
	 */
	void EmitImpact(UTRMassCombatSubsystem& Combat, const FVector& ImpactPoint,
		float Damage, float Radius, ETREnemyType SourceType, bool bExplosion, ETRTargetChannel Channel)
	{
		FTRMassImpactEvent Impact;
		Impact.Location = ImpactPoint;
		Impact.Damage = Damage;
		Impact.Radius = Radius;
		Impact.SourceType = SourceType;
		Impact.bExplosion = bExplosion;

		// 피해를 받을 액터는 이 채널의 목표에서 찾는다. 적이 쫓던 대상과
		// 피해를 받는 대상이 항상 같도록, 판단을 여기서 하지 않고 채널만 넘긴다.
		Impact.Channel = Channel;

		Combat.EnqueueImpact(Impact);
	}
}

UTREnemyAttackProcessor::UTREnemyAttackProcessor()
	: DetonationQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// 피해 판정은 서버 권위. 클라이언트는 복제된 표현만 본다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;

	// 행동 판단이므로 Behavior 그룹. 조종력을 만드는 Avoidance 그룹보다 앞이다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
}

void UTREnemyAttackProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// ── 폭발 예약 → 실행 ──────────────────────────────────────────────────
	DetonationQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	UE::TR::Mass::AddServerAuthoredRequirement<FTREnemyCombatFragment>(DetonationQuery, *this, EMassFragmentAccess::ReadOnly);
	// 피해를 받을 액터를 고르는 근거. 판단은 하지 않고 채널만 실어 보낸다.
	DetonationQuery.AddRequirement<FTRChaseChannelFragment>(EMassFragmentAccess::ReadOnly);
	DetonationQuery.AddConstSharedRequirement<FTREnemyCombatParams>(EMassFragmentPresence::All);
	UE::TR::Mass::AddServerAuthoredTagRequirement<FTRDetonatingTag>(DetonationQuery, *this, EMassFragmentPresence::All);

	// 준비 중에 죽은 개체는 터지지 않는다. 예약만 해두고 사살된 적이 시체 상태로
	// 폭발하면 플레이어가 막아낸 것이 무의미해진다.
	UE::TR::Mass::AddServerAuthoredTagRequirement<FTRDeadTag>(DetonationQuery, *this, EMassFragmentPresence::None);
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

	// 준비 시각이 지난 폭발을 실행한다. 예약은 StateTree 의 FTRDetonateTask 가 한다.
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

			EmitImpact(Combat, TransformList[EntityIt].GetTransform().GetLocation(),
				Combatant.ContactDamage, Params.ContactRadius, Params.EnemyType,
				/*bExplosion=*/true, ChannelList[EntityIt].Channel);

			// 자폭한 적은 자원을 주지 않는다(기획서). 그래서 여기서 자원 처리를 하지 않고
			// 그냥 지운다. 자원은 "처치"된 경우에만 나가야 한다.
			Context.Defer().DestroyEntity(EntityIt.GetEntityHandle());
		}
	});
}

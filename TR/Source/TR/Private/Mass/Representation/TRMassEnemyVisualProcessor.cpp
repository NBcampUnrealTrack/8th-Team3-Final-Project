// TRMassEnemyVisualProcessor.cpp

#include "Mass/Representation/TRMassEnemyVisualProcessor.h"

#include "Engine/World.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Net/TRMassNetContract.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Net/TRMassNetSnapshotProcessor.h"
#include "Mass/Representation/TRMassActorSyncProcessor.h"

DECLARE_CYCLE_STAT(TEXT("Enemy Visual"), STAT_TRMassEnemyVisual, STATGROUP_TRMass);

UTRMassEnemyVisualProcessor::UTRMassEnemyVisualProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// 체력이 있는 쪽에서만 돈다. 클라이언트에서 돌면 스폰 기본값(만피)으로 덮어써
	// 릴레이가 넣어준 값을 매 틱 지운다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;

	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;

	// 같은 프레임에 반영되려면 이 값을 읽는 쪽보다 앞이어야 한다. 뒤로 밀리면
	// 표현과 복제가 늘 한 프레임 늦은 손상도를 본다.
	ExecutionOrder.ExecuteBefore.Add(UTRMassNetSnapshotProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UTRMassActorSyncProcessor::StaticClass()->GetFName());

	// 프래그먼트만 만진다.
	bRequiresGameThreadExecution = false;
}

void UTRMassEnemyVisualProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	UE::TR::Mass::AddServerAuthoredRequirement<FTREnemyCombatFragment>(EntityQuery, *this, EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTREnemyVisualFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);
}

void UTRMassEnemyVisualProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRMassEnemyVisual);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRMassEnemyVisual);

	const UWorld* World = EntityManager.GetWorld();
	const float WorldTime = World ? World->GetTimeSeconds() : 0.f;

	EntityQuery.ForEachEntityChunk(Context, [WorldTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTREnemyCombatFragment> CombatList = Context.GetFragmentView<FTREnemyCombatFragment>();
		const TArrayView<FTREnemyVisualFragment> VisualList = Context.GetMutableFragmentView<FTREnemyVisualFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTREnemyCombatFragment& Combat = CombatList[EntityIt];
			FTREnemyVisualFragment& Visual = VisualList[EntityIt];

			// 체력 비율을 뒤집어 "얼마나 망가졌나"로 만든다. 0이 멀쩡함이어야
			// 값을 못 받은 쪽이 새 개체로 보인다(FTREnemyVisualFragment 주석 참고).
			const float NewRatio = (Combat.MaxHealth > 0.f)
				? 1.f - FMath::Clamp(Combat.Health / Combat.MaxHealth, 0.f, 1.f)
				: 0.f;

			// 값이 **올라간** 것만 피격으로 본다. 수리나 재사용으로 내려가는 경우
			// (풀에서 나온 개체가 만피로 초기화될 때)까지 번쩍이면 안 된다.
			if (NewRatio > Visual.DamageRatio + KINDA_SMALL_NUMBER)
			{
				Visual.LastHitTime = WorldTime;
			}

			Visual.DamageRatio = NewRatio;
		}
	});
}

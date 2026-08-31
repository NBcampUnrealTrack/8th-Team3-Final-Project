// TRMassNetSnapshotProcessor.cpp

#include "Mass/Net/TRMassNetSnapshotProcessor.h"

#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Net/TRMassNetRelay.h"

DECLARE_CYCLE_STAT(TEXT("Net Snapshot"), STAT_TRMassNetSnapshot, STATGROUP_TRMass);

UTRMassNetSnapshotProcessor::UTRMassNetSnapshotProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	// **Standalone 을 반드시 포함한다.**
	//
	// 보낼 것이 있는 쪽은 서버뿐이지만, 이 플래그는 "지금 서버인가"가 아니라
	// **월드가 만들어질 때 무엇이었나**로 걸러진다. 엔진은 파이프라인을 월드 시작 시
	// 한 번만 만들고(FMassProcessingPhaseManager::Initialize → DetermineProcessorExecutionFlags),
	// 그 뒤로는 다시 만들지 않는다.
	//
	// 이 게임은 메인 메뉴에서 맵 이동 없이 방을 연다(UTRGameInstance::StartListenInPlace 가
	// World->Listen() 만 부른다). 그래서 호스트의 월드는 **스탠드얼론으로 시작해서
	// 나중에 리슨 서버가 된다**. Server 만 적어두면 이 프로세서만 파이프라인에서 빠지고,
	// 스냅샷이 영원히 비어서 **클라이언트 화면에만 적이 통째로 안 나온다.**
	// 에러도 경고도 없다 — 프로세서가 아예 안 도는 것이라 로그를 남길 주체가 없다.
	//
	// PIE 리슨 서버로는 절대 재현되지 않는다. 그쪽은 월드가 처음부터 리슨 서버다.
	//
	// 실제로 보낼지 말지는 Execute 에서 매 틱 넷 모드를 보고 정한다. 그래야 방이
	// 열리는 순간 저절로 살아난다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;

	// 시뮬레이션 결과를 월드 밖으로 내보내는 단계.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;

	// 액터(릴레이)를 만진다.
	bRequiresGameThreadExecution = true;
}

void UTRMassNetSnapshotProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTRMassNetIdFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTREnemyStateFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

	// 손상도 복제용. 클라이언트에는 전투 프로세서가 없어 이 값이 유일한 출처다.
	EntityQuery.AddRequirement<FTREnemyVisualFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);
}

void UTRMassNetSnapshotProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_TRMassNetSnapshot);
	TRACE_CPUPROFILER_EVENT_SCOPE(TRMassNetSnapshot);

	UWorld* World = EntityManager.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 아직 방을 안 열었으면 보낼 상대가 없다. 넷 모드는 매 틱 다시 보므로
	// World->Listen() 이 성공하는 순간 다음 틱부터 저절로 스냅샷이 나가기 시작한다
	// (파이프라인은 그때 다시 만들어지지 않는다 — 생성자 주석 참고).
	if (World->GetNetMode() == NM_Standalone)
	{
		return;
	}

	ATRMassNetRelay* Relay = CachedRelay.Get();
	if (Relay == nullptr)
	{
		Relay = ATRMassNetRelay::Get(World, /*bCreateIfMissing=*/true);
		CachedRelay = Relay;

		if (Relay == nullptr)
		{
			return;
		}
	}

	const float WorldTime = World->GetTimeSeconds();
	if (WorldTime < NextSnapshotTime)
	{
		return;
	}
	NextSnapshotTime = WorldTime + Relay->GetSnapshotInterval();

	TArray<FTRMassAgentSnapshot> Snapshot;

	EntityQuery.ForEachEntityChunk(Context, [&Snapshot](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTRMassNetIdFragment> NetIdList = Context.GetFragmentView<FTRMassNetIdFragment>();
		const TConstArrayView<FTREnemyStateFragment> StateList = Context.GetFragmentView<FTREnemyStateFragment>();
		const TConstArrayView<FTREnemyVisualFragment> VisualList = Context.GetFragmentView<FTREnemyVisualFragment>();

		const bool bHasState = (StateList.Num() > 0);
		const bool bHasVisual = (VisualList.Num() > 0);

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTRMassNetIdFragment& NetId = NetIdList[EntityIt];
			if (NetId.NetId == 0)
			{
				// 아직 번호를 못 받은 개체. 스폰과 같은 프레임일 때 잠깐 생긴다.
				continue;
			}

			// 무엇을 어떻게 눌러 담을지는 스냅샷 구조체가 안다. 이 프로세서는 "어느
			// 엔티티를" 만 고른다 — 굽는 규칙이 되돌리는 규칙 바로 옆에 있어야
			// 필드를 늘릴 때 한쪽만 고치는 일이 줄어든다(FTRMassAgentSnapshot 주석 참고).
			FTRMassAgentSnapshot& Agent = Snapshot.AddDefaulted_GetRef();
			Agent.CollectFrom(
				TransformList[EntityIt].GetTransform(),
				NetId,
				bHasState ? &StateList[EntityIt] : nullptr,
				bHasVisual ? &VisualList[EntityIt] : nullptr);
		}
	});

	Relay->SubmitSnapshot(MoveTemp(Snapshot));
}

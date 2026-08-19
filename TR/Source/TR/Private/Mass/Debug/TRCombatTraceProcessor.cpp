// TRCombatTraceProcessor.cpp
//
// 임시 계측용. 지울 때는 이 파일과 짝 헤더만 지우면 된다.

#include "Mass/Debug/TRCombatTraceProcessor.h"

#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "Core/TRGameMode.h"
#include "Mass/Combat/TRMassCombatSubsystem.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Core/TRMassParams.h"
#include "Mass/Targeting/TRTargetDirectorSubsystem.h"
#include "Mass/Debug/TRMassDebug.h"

namespace
{
	static TAutoConsoleVariable<int32> CVarTraceCombat(
		TEXT("tr.Trace.Combat"), 0,
		TEXT("교전 계측 로그. 1이면 사격·자폭·관통을 LogTRMass 로 찍는다."),
		ECVF_Cheat);

	// 도달 후 이만큼 지나도 다음 상태로 안 넘어가면 굳은 것으로 본다.
	constexpr double StuckReportSeconds = 5.0;

	bool IsTracing()
	{
		return CVarTraceCombat.GetValueOnAnyThread() != 0;
	}

	// 로비 UI 없이 판을 시작한다. 계측 실행을 자동화하려고 둔다.
	static FAutoConsoleCommandWithWorld GStartMatchCommand(
		TEXT("tr.Trace.StartMatch"),
		TEXT("로비를 건너뛰고 판을 시작한다."),
		FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			ATRGameMode* GameMode = World ? World->GetAuthGameMode<ATRGameMode>() : nullptr;
			if (GameMode == nullptr)
			{
				UE_LOG(LogTRMass, Warning, TEXT("[계측] 판 시작 실패: 권한 있는 게임모드가 없다."));
				return;
			}

			UE_LOG(LogTRMass, Log, TEXT("[계측] 판 시작 요청 결과 = %d"), GameMode->StartMatch() ? 1 : 0);
		}));

	const TCHAR* LexAction(ETREnemyActionState State)
	{
		switch (State)
		{
		case ETREnemyActionState::Attack: return TEXT("자폭준비");
		case ETREnemyActionState::Launch: return TEXT("사출");
		case ETREnemyActionState::Fire:   return TEXT("사격");
		case ETREnemyActionState::Death:  return TEXT("사망");
		default:                          return TEXT("이동");
		}
	}
}

void UTRCombatTraceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 피해 적용 경로를 건드리지 않고 통지에만 붙는다.
	if (UTRMassCombatSubsystem* Combat = InWorld.GetSubsystem<UTRMassCombatSubsystem>())
	{
		Combat->OnEnemyImpact.AddUniqueDynamic(this, &UTRCombatTraceSubsystem::HandleImpact);
		Combat->OnEnemyKilled.AddUniqueDynamic(this, &UTRCombatTraceSubsystem::HandleKilled);
	}
}

void UTRCombatTraceSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UTRMassCombatSubsystem* Combat = World->GetSubsystem<UTRMassCombatSubsystem>())
		{
			Combat->OnEnemyImpact.RemoveDynamic(this, &UTRCombatTraceSubsystem::HandleImpact);
			Combat->OnEnemyKilled.RemoveDynamic(this, &UTRCombatTraceSubsystem::HandleKilled);
		}
	}

	Super::Deinitialize();
}

void UTRCombatTraceSubsystem::HandleImpact(const FTRMassImpactEvent& Impact)
{
	if (!IsTracing())
	{
		return;
	}

	UE_LOG(LogTRMass, Log, TEXT("[계측] 피격 원인=%s 피해=%.0f 폭발=%d 위치=(%.0f, %.0f, %.0f)"),
		LexToString(Impact.SourceType), Impact.Damage, Impact.bExplosion ? 1 : 0,
		Impact.Location.X, Impact.Location.Y, Impact.Location.Z);
}

void UTRCombatTraceSubsystem::HandleKilled(const FVector& Location, int32 ResourceValue)
{
	if (!IsTracing())
	{
		return;
	}

	UE_LOG(LogTRMass, Log, TEXT("[계측] 처치 자원=%d 위치=(%.0f, %.0f, %.0f)"),
		ResourceValue, Location.X, Location.Y, Location.Z);
}

UTRCombatTraceProcessor::UTRCombatTraceProcessor()
	: TraceQuery(*this)
{
#if UE_BUILD_SHIPPING
	bAutoRegisterWithProcessingPhases = false;
#else
	bAutoRegisterWithProcessingPhases = true;
#endif

	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;

	// 이동과 상태 갱신이 모두 끝난 값을 본다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;

	// TMap 을 들고 있어 병렬로 돌리면 안 된다.
	bRequiresGameThreadExecution = true;
}

void UTRCombatTraceProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	TraceQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	TraceQuery.AddRequirement<FTREnemyStateFragment>(EMassFragmentAccess::ReadOnly);
	TraceQuery.AddRequirement<FTREnemyCombatFragment>(EMassFragmentAccess::ReadOnly);
	TraceQuery.AddRequirement<FTRTargetFragment>(EMassFragmentAccess::ReadOnly);
	TraceQuery.AddRequirement<FTRApproachFragment>(EMassFragmentAccess::ReadOnly);
	TraceQuery.AddRequirement<FTRChaseChannelFragment>(EMassFragmentAccess::ReadOnly);
	TraceQuery.AddConstSharedRequirement<FTREnemyCombatParams>(EMassFragmentPresence::All);
	TraceQuery.AddSubsystemRequirement<UTRTargetDirectorSubsystem>(EMassFragmentAccess::ReadOnly);
	TraceQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);
}

void UTRCombatTraceProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!IsTracing())
	{
		if (LastSeen.Num() > 0)
		{
			LastSeen.Empty();
		}
		return;
	}

	TSet<FMassEntityHandle> Alive;

	TraceQuery.ForEachEntityChunk(Context, [this, &Alive](FMassExecutionContext& Chunk)
	{
		const FTREnemyCombatParams& Params = Chunk.GetConstSharedFragment<FTREnemyCombatParams>();
		const UTRTargetDirectorSubsystem& Director = Chunk.GetSubsystemChecked<UTRTargetDirectorSubsystem>();

		FTRResolvedTarget Shapes[static_cast<int32>(ETRTargetChannel::MAX)];
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Shapes); ++Index)
		{
			Director.GetResolvedTarget(static_cast<ETRTargetChannel>(Index), Shapes[Index]);
		}

		const TConstArrayView<FTransformFragment> Transforms = Chunk.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTREnemyStateFragment> States = Chunk.GetFragmentView<FTREnemyStateFragment>();
		const TConstArrayView<FTREnemyCombatFragment> Combats = Chunk.GetFragmentView<FTREnemyCombatFragment>();
		const TConstArrayView<FTRTargetFragment> Targets = Chunk.GetFragmentView<FTRTargetFragment>();
		const TConstArrayView<FTRApproachFragment> Approaches = Chunk.GetFragmentView<FTRApproachFragment>();
		const TConstArrayView<FTRChaseChannelFragment> Channels = Chunk.GetFragmentView<FTRChaseChannelFragment>();

		for (FMassExecutionContext::FEntityIterator It = Chunk.CreateEntityIterator(); It; ++It)
		{
			const FMassEntityHandle Entity = It.GetEntityHandle();
			Alive.Add(Entity);

			const FVector Self = Transforms[It].GetTransform().GetLocation();
			const FTRTargetFragment& Target = Targets[It];

			// 도달 판정은 수평 거리로만 한다. 같은 기준으로 재야 로그와 동작이 맞는다.
			FVector ToTarget = Target.TargetLocation - Self;
			const float HeightGap = static_cast<float>(ToTarget.Z);
			ToTarget.Z = 0.f;
			const float Distance = Target.bHasTarget ? static_cast<float>(ToTarget.Size()) : -1.f;

			FSeen& Seen = LastSeen.FindOrAdd(Entity);

			// 사격 — 한 발 나갈 때마다 몇 발째인지 찍는다.
			if (Combats[It].ShotsFired != Seen.ShotsFired)
			{
				UE_LOG(LogTRMass, Log, TEXT("[계측] %s 사격 %d/%d 발  수평 %.0fcm 높이차 %.0fcm (유지거리 %.0f)"),
					LexToString(Params.EnemyType), Combats[It].ShotsFired, Params.ShotCount,
					Distance, HeightGap, Approaches[It].StopDistance);
				Seen.ShotsFired = Combats[It].ShotsFired;
			}

			// 상태 전이 — 자폭 준비에 들어간 거리가 정지 거리와 얼마나 벌어지는지 본다.
			if (States[It].ActionState != Seen.ActionState)
			{
				UE_LOG(LogTRMass, Log, TEXT("[계측] %s %s -> %s  수평 %.0fcm 높이차 %.0fcm (정지거리 %.0f, 도달=%d)"),
					LexToString(Params.EnemyType), LexAction(Seen.ActionState),
					LexAction(States[It].ActionState), Distance, HeightGap,
					Approaches[It].StopDistance, Target.bArrived ? 1 : 0);
				Seen.ActionState = States[It].ActionState;
			}

			// 관통 — 차체 바운드 안에 들어가 있으면 기차를 뚫고 지나간 것이다.
			bool bInsideHull = false;
			const int32 ChannelIndex = static_cast<int32>(Channels[It].Channel);
			if (ChannelIndex >= 0 && ChannelIndex < UE_ARRAY_COUNT(Shapes))
			{
				const FTRResolvedTarget& Shape = Shapes[ChannelIndex];
				if (Shape.bValid && Shape.bHasBounds)
				{
					const FVector Local = Shape.Transform.InverseTransformPosition(Self);
					bInsideHull = Shape.LocalBounds.IsInsideXY(Local);
				}
			}

			// 굳음 — 정지 거리 안인데 자폭·사격으로 넘어가지 않고 머무는 개체.
			const bool bAtTarget = (Distance >= 0.f) && (Distance <= Approaches[It].StopDistance)
				&& (States[It].ActionState == ETREnemyActionState::None);
			if (!bAtTarget)
			{
				Seen.ArrivedSince = 0.0;
				Seen.bReportedStuck = false;
			}
			else
			{
				const double Now = Chunk.GetWorld() ? Chunk.GetWorld()->GetTimeSeconds() : 0.0;
				if (Seen.ArrivedSince <= 0.0)
				{
					Seen.ArrivedSince = Now;
				}
				else if (!Seen.bReportedStuck && (Now - Seen.ArrivedSince) > StuckReportSeconds)
				{
					UE_LOG(LogTRMass, Warning, TEXT("[계측] %s 굳음  도달 후 %.1f초 정지  수평 %.0fcm (정지거리 %.0f)"),
						LexToString(Params.EnemyType), Now - Seen.ArrivedSince, Distance, Approaches[It].StopDistance);
					Seen.bReportedStuck = true;
				}
			}

			if (bInsideHull != Seen.bInsideHull)
			{
				if (bInsideHull)
				{
					UE_LOG(LogTRMass, Warning, TEXT("[계측] %s 차체 관통  표적까지 %.0fcm (정지거리 %.0f)"),
						LexToString(Params.EnemyType), Distance, Approaches[It].StopDistance);
				}
				Seen.bInsideHull = bInsideHull;
			}
		}
	});

	// 사라진 개체는 표를 비워 준다. 안 그러면 핸들이 재사용될 때 옛 값과 비교한다.
	for (auto It = LastSeen.CreateIterator(); It; ++It)
	{
		if (!Alive.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

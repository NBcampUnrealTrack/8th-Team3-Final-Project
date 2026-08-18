// TRWaveDirectorSubsystem.cpp

#include "Mass/Spawning/TRWaveDirectorSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Mass/Debug/TRMassDebug.h"
#include "TimerManager.h"

void UTRWaveDirectorSubsystem::Deinitialize()
{
	StopWave();

	Super::Deinitialize();
}

void UTRWaveDirectorSubsystem::StartWave(const FTRWaveDefinition& WaveDefinition, AActor* TargetActor)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (WaveDefinition.Groups.IsEmpty())
	{
		UE_LOG(LogTRMass, Warning, TEXT("웨이브 시작 실패: 그룹이 하나도 없습니다."));
		return;
	}

	// 진행 중이던 웨이브는 정리한다. 타이머가 겹치면 배치가 뒤섞인다.
	StopWave();

	CurrentWave = WaveDefinition;
	CurrentTarget = TargetActor;
	bWaveActive = true;

	if (InitializeGroupStates() == 0)
	{
		UE_LOG(LogTRMass, Warning, TEXT("웨이브 시작 실패: 스폰 가능한 그룹이 없습니다."));
		bWaveActive = false;
		return;
	}

	// 타이머를 먼저 다 걸어두고, 그 다음에 지연 없는 그룹의 첫 배치를 낸다.
	// 즉시 스폰이 웨이브를 끝내버리는 경우(배치 1회짜리 그룹만 있는 웨이브)에도
	// 타이머가 남지 않도록 순서를 이렇게 잡는다.
	ScheduleGroupTimers(*World);
	SpawnUndelayedGroups();
}

int32 UTRWaveDirectorSubsystem::InitializeGroupStates()
{
	GroupStates.Reset();
	GroupStates.SetNum(CurrentWave.Groups.Num());

	int32 ValidGroupCount = 0;

	for (int32 GroupIndex = 0; GroupIndex < CurrentWave.Groups.Num(); ++GroupIndex)
	{
		const FTRWaveGroup& Group = CurrentWave.Groups[GroupIndex];
		FTRWaveGroupRuntime& State = GroupStates[GroupIndex];

		State.BatchesRemaining = Group.BatchCount;
		State.SpawnedTotal = 0;
		State.TotalLimit = FMath::Max(0, Group.TotalCount);
		State.PendingSpawns = 0;
		State.DrainDeadline = 0.f;

		// Config가 없는 그룹은 조용히 넘어가면 원인을 찾기 어렵다. 여기서 한 번 짚고 끈다.
		State.bFinished = (Group.EnemyConfig == nullptr);
		if (State.bFinished)
		{
			UE_LOG(LogTRMass, Warning, TEXT("웨이브 그룹 %d(%s): EnemyConfig가 비어 있어 건너뜁니다."),
				GroupIndex, LexToString(Group.EnemyType));
			continue;
		}

		++ValidGroupCount;

		UE_LOG(LogTRMass, Log, TEXT("웨이브 그룹 %d(%s): %d마리 × %s배치, 간격 %.1f초, 지연 %.1f초, 총량 %s"),
			GroupIndex, LexToString(Group.EnemyType), Group.BatchSize,
			Group.BatchCount > 0 ? *FString::FromInt(Group.BatchCount) : TEXT("무한 "),
			Group.BatchInterval, Group.StartDelay,
			State.TotalLimit > 0 ? *FString::FromInt(State.TotalLimit) : TEXT("무제한"));
	}

	return ValidGroupCount;
}

void UTRWaveDirectorSubsystem::ScheduleGroupTimers(UWorld& World)
{
	for (int32 GroupIndex = 0; GroupIndex < CurrentWave.Groups.Num(); ++GroupIndex)
	{
		if (GroupStates[GroupIndex].bFinished)
		{
			continue;
		}

		const FTRWaveGroup& Group = CurrentWave.Groups[GroupIndex];

		// 지연이 없는 그룹은 첫 배치를 SpawnUndelayedGroups가 즉시 내므로,
		// 타이머의 첫 발화는 한 간격 뒤가 된다.
		const float FirstDelay = (Group.StartDelay > 0.f) ? Group.StartDelay : Group.BatchInterval;

		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(
			this, &UTRWaveDirectorSubsystem::SpawnGroupBatch, GroupIndex);

		World.GetTimerManager().SetTimer(GroupStates[GroupIndex].TimerHandle, Delegate,
			Group.BatchInterval, /*bLoop=*/true, FirstDelay);
	}
}

void UTRWaveDirectorSubsystem::SpawnUndelayedGroups()
{
	for (int32 GroupIndex = 0; GroupIndex < CurrentWave.Groups.Num(); ++GroupIndex)
	{
		// 앞선 그룹의 즉시 스폰으로 웨이브가 이미 끝났을 수 있다.
		if (!bWaveActive)
		{
			break;
		}

		if (!GroupStates.IsValidIndex(GroupIndex) || GroupStates[GroupIndex].bFinished)
		{
			continue;
		}

		if (CurrentWave.Groups[GroupIndex].StartDelay <= 0.f)
		{
			// 웨이브 시작이 눈에 보여야 하므로 첫 배치는 기다리지 않는다.
			SpawnGroupBatch(GroupIndex);
		}
	}
}

void UTRWaveDirectorSubsystem::StopWave()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		for (FTRWaveGroupRuntime& State : GroupStates)
		{
			TimerManager.ClearTimer(State.TimerHandle);
			State.bFinished = true;
		}
	}

	bWaveActive = false;
}

int32 UTRWaveDirectorSubsystem::GetRemainingBatches() const
{
	if (!bWaveActive)
	{
		return 0;
	}

	int32 Total = 0;
	for (int32 GroupIndex = 0; GroupIndex < GroupStates.Num(); ++GroupIndex)
	{
		if (GroupStates[GroupIndex].bFinished)
		{
			continue;
		}

		const FTRWaveGroup& Group = CurrentWave.Groups[GroupIndex];
		if (Group.BatchCount <= 0)
		{
			// 무한 그룹이 하나라도 있으면 합계에 의미가 없다.
			return -1;
		}

		Total += GroupStates[GroupIndex].BatchesRemaining;

		// 밀린 물량도 아직 나올 적이다. 이걸 빼면 "남은 배치 0"인데 적이 더 쏟아져서,
		// 이 값으로 진행도를 그리는 UI가 웨이브가 끝난 것처럼 보여준다.
		if (Group.BatchSize > 0)
		{
			Total += FMath::DivideAndRoundUp(GroupStates[GroupIndex].PendingSpawns, Group.BatchSize);
		}
	}

	return Total;
}

int32 UTRWaveDirectorSubsystem::GetAliveCount() const
{
	const UWorld* World = GetWorld();
	UTRMassSpawnerSubsystem* Spawner = World ? World->GetSubsystem<UTRMassSpawnerSubsystem>() : nullptr;

	return Spawner ? Spawner->GetSpawnedCount() : 0;
}

FTRWaveDefinition UTRWaveDirectorSubsystem::ScaleWaveForNumber(const FTRWaveDefinition& BaseDefinition,
	int32 WaveNumber, int32 SourceWaveNumber, int32 ScalingStartWave, float PerWaveMultiplier)
{
	FTRWaveDefinition Scaled = BaseDefinition;

	// 정액 증가는 "테이블에 적힌 웨이브에서 몇 칸 넘어왔는가"로 센다.
	// 기획서의 사출형(4×(웨이브−2))·사격형(웨이브당 +1)이 선형이기 때문이다.
	const int32 LinearSteps = FMath::Max(0, WaveNumber - SourceWaveNumber);
	if (LinearSteps > 0)
	{
		for (int32 GroupIndex = 0; GroupIndex < Scaled.Groups.Num(); ++GroupIndex)
		{
			FTRWaveGroup& Group = Scaled.Groups[GroupIndex];
			if (Group.TotalCountPerWave == 0)
			{
				continue;
			}

			// TotalCount가 0이면 "총량 제한 없음"이라 더할 기준이 없다. 웨이브당 증가를
			// 적어놓고 기준값을 빠뜨리면 값을 넣었는데 아무 반응이 없어 원인을 찾기 어렵다.
			if (Group.TotalCount <= 0)
			{
				UE_LOG(LogTRMass, Warning,
					TEXT("웨이브 그룹 %d(%s): TotalCountPerWave(%d)가 설정됐지만 TotalCount가 0이라 무시됩니다. ")
					TEXT("기준이 되는 마릿수를 TotalCount에 넣으세요."),
					GroupIndex, LexToString(Group.EnemyType), Group.TotalCountPerWave);
				continue;
			}

			Group.TotalCount = FMath::Max(1, Group.TotalCount + Group.TotalCountPerWave * LinearSteps);
		}
	}

	// 복리 증가는 기획서 기준 웨이브 번호(T4+ = W10)부터 적용한다.
	const int32 CompoundSteps = WaveNumber - ScalingStartWave;
	if (CompoundSteps <= 0 || PerWaveMultiplier <= 0.f)
	{
		return Scaled;
	}

	const float Multiplier = FMath::Pow(PerWaveMultiplier, static_cast<float>(CompoundSteps));

	// 복리는 체력·피해에만 건다. 속도와 자원은 기획서상 T3 에서 고정이다.
	Scaled.StatScale.Health = BaseDefinition.StatScale.Health * Multiplier;
	Scaled.StatScale.Damage = BaseDefinition.StatScale.Damage * Multiplier;

	for (int32 GroupIndex = 0; GroupIndex < Scaled.Groups.Num(); ++GroupIndex)
	{
		const FTRWaveGroup& BaseGroup = BaseDefinition.Groups[GroupIndex];
		if (!BaseGroup.bScaleBatchSize)
		{
			continue;
		}

		Scaled.Groups[GroupIndex].BatchSize = FMath::Max(1, FMath::RoundToInt(BaseGroup.BatchSize * Multiplier));
	}

	// 동시 상한은 성능 예산이므로 함께 키우지 않는다.
	// 물량이 늘수록 상한에 먼저 걸리게 두는 것이 의도된 동작이다.
	return Scaled;
}

void UTRWaveDirectorSubsystem::SpawnGroupBatch(int32 GroupIndex)
{
	if (!bWaveActive || !GroupStates.IsValidIndex(GroupIndex) || !CurrentWave.Groups.IsValidIndex(GroupIndex))
	{
		return;
	}

	FTRWaveGroupRuntime& State = GroupStates[GroupIndex];
	if (State.bFinished)
	{
		return;
	}

	const FTRWaveGroup& Group = CurrentWave.Groups[GroupIndex];

	UWorld* World = GetWorld();
	UTRMassSpawnerSubsystem* Spawner = World ? World->GetSubsystem<UTRMassSpawnerSubsystem>() : nullptr;
	if (Spawner == nullptr)
	{
		StopWave();
		return;
	}

	// ── 1. 스케줄: 이번 주기에 나왔어야 할 물량을 빚에 더한다 ──────────────
	//
	// 배치 스케줄과 실제 스폰을 분리한다. 예전에는 상한에 막히면 그 배치를 그냥
	// 소진시켰는데, 그러면 **밀리는 플레이어일수록 적이 덜 나오는** 암묵적인 난이도
	// 감면이 된다. 성능 예산이 난이도 조절 장치로 새는 셈이라, 웨이브 총량이
	// 기획서와 조용히 달라진다.
	const bool bLimitedBatches = (Group.BatchCount > 0);
	const bool bHasScheduledBatch = !bLimitedBatches || (State.BatchesRemaining > 0);

	if (bHasScheduledBatch)
	{
		State.PendingSpawns += Group.BatchSize;

		// 빚에 상한을 둔다. 없으면 오래 밀린 뒤 한숨 돌리는 순간 수백 마리가
		// 한꺼번에 쏟아져 프레임도 플레이어도 못 버틴다.
		// MaxPendingBatches가 0이면 배수가 1이 되어 이월이 없다(예전 동작).
		const int32 PendingCap = Group.BatchSize * FMath::Max(1, Group.MaxPendingBatches);
		State.PendingSpawns = FMath::Min(State.PendingSpawns, PendingCap);

		// 스케줄은 상한과 무관하게 진행한다. 이게 리듬을 정하는 값이고,
		// 물량은 아래에서 따로 정산된다.
		if (bLimitedBatches)
		{
			--State.BatchesRemaining;
		}
	}

	// 총량 상한이 있으면 빚도 그 안으로 자른다. 안 자르면 남은 빚 때문에
	// 아래 종료 판정이 총량을 다 채우고도 계속 기다린다.
	if (State.TotalLimit > 0)
	{
		State.PendingSpawns = FMath::Min(State.PendingSpawns, State.TotalLimit - State.SpawnedTotal);
	}

	// ── 2. 정산: 성능 예산이 허락하는 만큼만 실제로 낸다 ──────────────────
	//
	// 동시 생존 상한은 웨이브 전체 기준이다. 그룹별로 나누면 종류가 늘어날 때마다
	// 총량이 함께 늘어나 성능 예산이 무너진다.
	const int32 AliveCount = Spawner->GetSpawnedCount();
	const int32 Allowance = FMath::Max(0, CurrentWave.MaxConcurrent - AliveCount);

	const int32 SpawnCount = FMath::Min(State.PendingSpawns, Allowance);

	if (SpawnCount > 0)
	{
		FTRChaserSpawnParams SpawnParams;
		SpawnParams.EnemyConfig = Group.EnemyConfig;
		SpawnParams.Count = SpawnCount;
		SpawnParams.Channel = CurrentWave.Channel;
		SpawnParams.StatScale = CurrentWave.StatScale;

		ResolveSpawnShape(Group, SpawnParams.Area);

		const int32 Spawned = Spawner->SpawnChasers(SpawnParams);
		State.SpawnedTotal += Spawned;
		State.PendingSpawns = FMath::Max(0, State.PendingSpawns - Spawned);

		UE_LOG(LogTRMass, Verbose, TEXT("그룹 %d(%s) 배치: %d마리 (생존 %d / 상한 %d, 누적 %d, 밀림 %d)"),
			GroupIndex, LexToString(Group.EnemyType), Spawned,
			AliveCount + Spawned, CurrentWave.MaxConcurrent, State.SpawnedTotal, State.PendingSpawns);
	}
	else if (State.PendingSpawns > 0)
	{
		UE_LOG(LogTRMass, Verbose, TEXT("그룹 %d(%s) 대기: 동시 상한 도달 (생존 %d / 상한 %d, 밀림 %d)"),
			GroupIndex, LexToString(Group.EnemyType), AliveCount, CurrentWave.MaxConcurrent,
			State.PendingSpawns);
	}

	// ── 3. 종료 판정 ──────────────────────────────────────────────────────
	//
	// 이 아래에서 FinishGroup을 부른 뒤에는 GroupStates를 건드리지 않는다 —
	// 웨이브 종료 알림을 받은 쪽이 곧바로 다음 웨이브를 시작할 수 있다.

	if ((State.TotalLimit > 0) && (State.SpawnedTotal >= State.TotalLimit))
	{
		FinishGroup(GroupIndex);
		return;
	}

	// 배치가 남아 있으면 계속 돈다. 무한 그룹도 여기서 걸러진다.
	if (!bLimitedBatches || State.BatchesRemaining > 0)
	{
		return;
	}

	if (State.PendingSpawns <= 0)
	{
		FinishGroup(GroupIndex);
		return;
	}

	// 배치는 다 냈는데 빚이 남았다. 타이머는 계속 돌고 있으므로 다음 주기에 마저 낸다.
	//
	// 다만 플레이어가 끝내 처리하지 못하면 빚이 영원히 안 빠져 웨이브가 끝나지 않고,
	// 휴식도 다음 웨이브도 오지 않아 게임이 멈춘 것처럼 보인다. 기다리는 시간에 상한을 둔다.
	const float WorldTime = World->GetTimeSeconds();
	if (State.DrainDeadline <= 0.f)
	{
		State.DrainDeadline = WorldTime + FMath::Max(0.f, CurrentWave.PendingDrainTime);
	}

	if (WorldTime >= State.DrainDeadline)
	{
		UE_LOG(LogTRMass, Log, TEXT("그룹 %d(%s): 밀린 물량 %d마리를 내보내지 못하고 종료합니다 (대기 %.1f초 초과)."),
			GroupIndex, LexToString(Group.EnemyType), State.PendingSpawns, CurrentWave.PendingDrainTime);

		FinishGroup(GroupIndex);
	}
}

void UTRWaveDirectorSubsystem::FinishGroup(int32 GroupIndex)
{
	if (!GroupStates.IsValidIndex(GroupIndex))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(GroupStates[GroupIndex].TimerHandle);
	}

	GroupStates[GroupIndex].bFinished = true;

	for (const FTRWaveGroupRuntime& State : GroupStates)
	{
		if (!State.bFinished)
		{
			return;
		}
	}

	// 모든 그룹이 배치를 다 냈다. 적이 다 죽은 시점이 아니라는 점에 주의.
	bWaveActive = false;

	// 알림은 다음 틱에 보낸다. 받은 쪽이 곧바로 다음 웨이브를 시작하면
	// 아직 스택에 남아 있는 스폰 루프가 갈아엎힌 GroupStates 를 옛 인덱스로 다시 만진다.
	if (!ensureMsgf(World, TEXT("웨이브 종료 알림 실패 — 월드가 없습니다.")))
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]() { OnWaveFinished.Broadcast(); }));
}

void UTRWaveDirectorSubsystem::ResolveSpawnShape(const FTRWaveGroup& Group, FTRSpawnAreaParams& OutParams) const
{
	// 기준 방향은 대상 액터의 정면(기차 진행 방향)이다.
	const float ForwardYaw = CurrentTarget.IsValid() ? CurrentTarget->GetActorRotation().Yaw : 0.f;

	// 각 구역은 90도 호이고, 아래 값은 그 호의 중심 각도다.
	// Sides는 좌(+90)를 기준으로 잡고 대칭 플래그로 우(-90)를 함께 쓴다.
	float DirectionOffset = 0.f;
	switch (Group.Direction)
	{
	case ETRSpawnDirection::Back:  DirectionOffset = 180.f; break;
	case ETRSpawnDirection::Left:  DirectionOffset = 90.f;  break;
	case ETRSpawnDirection::Right: DirectionOffset = -90.f; break;
	case ETRSpawnDirection::Sides: DirectionOffset = 90.f;  break;
	default: break;
	}

	OutParams.Origin = CurrentTarget.IsValid() ? CurrentTarget->GetActorLocation() : FVector::ZeroVector;

	// 방위를 축 + 오프셋으로 나눠 둔다. 모양 계산은 그 합만 보므로,
	// 모양을 추가해도 방위 처리를 다시 짤 일이 없다.
	OutParams.OrientationYawDegrees = ForwardYaw;
	OutParams.DirectionOffsetDegrees = DirectionOffset;
	OutParams.bMirrorDirection = (Group.Direction == ETRSpawnDirection::Sides);

	// 모양·크기는 사람이 정한 값 그대로다. 옮겨 담지 않는다.
	OutParams.ShapeParams = Group.ShapeParams;

	// 나머지는 "방위 + 스폰 거리"라는 웨이브의 어휘를 스폰 계산의 어휘로 환산한 것이다.
	//
	// 고리 — 방위 축을 중심으로 좌우 45도씩. 전방위면 축과 무관하게 한 바퀴.
	constexpr float ArcWidth = 90.f;
	const bool bFullCircle = (Group.Direction == ETRSpawnDirection::All);

	OutParams.InnerRadius = Group.SpawnDistance;
	OutParams.OuterRadius = Group.SpawnDistance + Group.SpawnDistanceJitter;
	OutParams.AngleRangeDegrees = bFullCircle ? 360.f : ArcWidth;
	OutParams.AngleStartDegrees = bFullCircle ? -180.f : -(ArcWidth * 0.5f);

	// 사각·선 — 방위 방향으로 스폰 거리만큼 밀어낸 자리에 놓는다.
	OutParams.ShapeOffset = Group.SpawnDistance;
}

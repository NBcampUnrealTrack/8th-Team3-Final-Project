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

		if (CurrentWave.Groups[GroupIndex].BatchCount <= 0)
		{
			// 무한 그룹이 하나라도 있으면 합계에 의미가 없다.
			return -1;
		}

		Total += GroupStates[GroupIndex].BatchesRemaining;
	}

	return Total;
}

int32 UTRWaveDirectorSubsystem::GetAliveCount() const
{
	const UWorld* World = GetWorld();
	const UTRMassSpawnerSubsystem* Spawner = World ? World->GetSubsystem<UTRMassSpawnerSubsystem>() : nullptr;

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

	// 스탯 배수는 그룹과 무관하게 웨이브 전체에 걸린다.
	Scaled.StatScale = BaseDefinition.StatScale * Multiplier;

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

	// 동시 생존 상한은 웨이브 전체 기준이다. 그룹별로 나누면 종류가 늘어날 때마다
	// 총량이 함께 늘어나 성능 예산이 무너진다.
	const int32 AliveCount = Spawner->GetSpawnedCount();
	const int32 Allowance = FMath::Max(0, CurrentWave.MaxConcurrent - AliveCount);

	int32 SpawnCount = FMath::Min(Group.BatchSize, Allowance);
	if (State.TotalLimit > 0)
	{
		SpawnCount = FMath::Min(SpawnCount, State.TotalLimit - State.SpawnedTotal);
	}

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

		UE_LOG(LogTRMass, Verbose, TEXT("그룹 %d(%s) 배치: %d마리 (생존 %d / 상한 %d, 누적 %d)"),
			GroupIndex, LexToString(Group.EnemyType), Spawned,
			AliveCount + Spawned, CurrentWave.MaxConcurrent, State.SpawnedTotal);
	}
	else
	{
		UE_LOG(LogTRMass, Verbose, TEXT("그룹 %d(%s) 배치 건너뜀: 동시 상한 도달 (생존 %d / 상한 %d)"),
			GroupIndex, LexToString(Group.EnemyType), AliveCount, CurrentWave.MaxConcurrent);
	}

	// 상한에 걸려 못 냈어도 배치는 소진시킨다. 그래야 웨이브가 끝날 수 있다.
	const bool bLimitedBatches = (Group.BatchCount > 0);
	if (bLimitedBatches)
	{
		--State.BatchesRemaining;
	}

	const bool bBatchesDone = bLimitedBatches && (State.BatchesRemaining <= 0);
	const bool bTotalDone = (State.TotalLimit > 0) && (State.SpawnedTotal >= State.TotalLimit);

	if (bBatchesDone || bTotalDone)
	{
		// 이 호출 뒤에는 GroupStates를 건드리지 않는다 —
		// 웨이브 종료 알림을 받은 쪽이 곧바로 다음 웨이브를 시작할 수 있다.
		FinishGroup(GroupIndex);
	}
}

void UTRWaveDirectorSubsystem::FinishGroup(int32 GroupIndex)
{
	if (!GroupStates.IsValidIndex(GroupIndex))
	{
		return;
	}

	if (UWorld* World = GetWorld())
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
	OnWaveFinished.Broadcast();
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

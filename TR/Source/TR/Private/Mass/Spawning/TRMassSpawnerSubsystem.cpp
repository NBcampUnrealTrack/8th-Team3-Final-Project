// TRMassSpawnerSubsystem.cpp

#include "Mass/Spawning/TRMassSpawnerSubsystem.h"

#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassEntityTemplate.h"
#include "MassEntityUtils.h"
#include "MassMovementFragments.h"
#include "MassSpawnerSubsystem.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Core/TRMassParams.h"
#include "Mass/Net/TRMassNetRelay.h"
#include "Mass/Spawning/TRMassSpawnUtils.h"

namespace
{
	/**
	 * 이 배치가 내려는 최고 속도가 엔진 클램프(Mass Movement의 Max Speed)를 넘는지 검사한다.
	 *
	 * 넘으면 UMassApplyForceProcessor가 잘라내는데, 에러도 경고도 없이 그냥 느리게
	 * 움직일 뿐이라 원인을 찾기 매우 어렵다. Config를 잘못 채운 순간 알려준다.
	 *
	 * **티어 배수(StatScale.Speed)를 반드시 함께 본다.** 예전에는 Config에 적힌 단계
	 * 속도만 검사했는데, 그러면 가장 흔한 실수를 통째로 놓친다 — SpeedRatio 기본값이
	 * 1.0이라 기준 속도가 이미 천장이고, 거기에 티어 배수를 곱하면 전부 잘린다.
	 * 즉 기획서의 T1→T3 속도 성장(5→10 m/s)이 조용히 사라지는데 경고는 안 나왔다.
	 *
	 * 해결책이 두 개라 메시지에 둘 다 적는다. Max Speed는 개체별로 바꿀 수 없는
	 * 공유 파라미터라, **Max Speed를 최고 티어 속도로 잡고 SpeedRatio로 T1을 만드는**
	 * 방향이 정답이다.
	 */
	void WarnIfSpeedExceedsMovementLimit(const FMassEntityManager& EntityManager, const FMassEntityHandle& Entity,
		const FTREnemyCombatParams& CombatParams, const FTRChaseParams* ChaseParams,
		const FTREnemyStatScale& StatScale, const UMassEntityConfigAsset* Config)
	{
		const FMassMovementParameters* MovementParams =
			EntityManager.GetConstSharedFragmentDataPtr<FMassMovementParameters>(Entity);

		if (MovementParams == nullptr || ChaseParams == nullptr)
		{
			return;
		}

		const float PeakSpeed = UE::TR::Enemy::ComputePeakSpeed(
			CombatParams, *ChaseParams, MovementParams->MaxSpeed, StatScale);

		// 같은 값이면 정상이다(Max Speed를 천장으로 삼는 구성). 부동소수 오차만 봐준다.
		if (PeakSpeed <= MovementParams->MaxSpeed * (1.f + KINDA_SMALL_NUMBER))
		{
			return;
		}

		UE_LOG(LogTRMass, Warning,
			TEXT("'%s'(%s): 이 배치가 내려는 최고 속도 %.0f cm/s가 Mass Movement의 Max Speed(%.0f)를 넘습니다. ")
			TEXT("엔진이 잘라내므로 설정한 속도가 나오지 않습니다 — 티어를 올려도 빨라지지 않습니다. ")
			TEXT("(단계 속도 %.0f x 스탯 배수 %.2f, SpeedRatio %.2f) ")
			TEXT("Max Speed를 최고 티어 속도(%.0f 이상)로 올리고, T1 속도는 SpeedRatio를 내려 만드세요."),
			*GetNameSafe(Config), LexToString(CombatParams.EnemyType),
			PeakSpeed, MovementParams->MaxSpeed,
			PeakSpeed / FMath::Max(StatScale.Speed, 0.01f), FMath::Max(StatScale.Speed, 0.01f),
			ChaseParams->SpeedRatio,
			PeakSpeed);
	}

	/**
	 * 갓 만들어진 엔티티에 스폰 시점 값을 채운다.
	 *
	 * 프래그먼트를 하나씩 확인하는 이유는 Config가 TR Chaser 트레이트를 쓰지 않을 수도
	 * 있기 때문이다. 없는 건 건너뛰고, 있는 것만 채운다.
	 *
	 * @param bReportConfigIssues 설정 오류 검사를 할지. 배치의 첫 개체에서만 켜서
	 *                            같은 경고가 마릿수만큼 찍히는 걸 막는다.
	 */
	void InitializeSpawnedEntity(FMassEntityManager& EntityManager, const FMassEntityHandle& Entity,
		const FTRChaserSpawnParams& Params, const FVector& Location, FRandomStream& RandomStream,
		bool bReportConfigIssues, const UMassEntityConfigAsset* Config,
		int32 NetId, uint8 ConfigIndex)
	{
		if (FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
		{
			TransformFragment->GetMutableTransform().SetLocation(Location);
		}

		const FTRChaseParams* ChaseParams = EntityManager.GetConstSharedFragmentDataPtr<FTRChaseParams>(Entity);

		// 개체별 속도 배수. 이걸 안 뽑으면 한 무리가 통째로 같은 속도로 움직여서
		// 대열이 얼어붙은 것처럼 보인다. 스폰 시 한 번만 정하고 이후 바꾸지 않는다.
		{
			FTRApproachFragment* ApproachFragment = EntityManager.GetFragmentDataPtr<FTRApproachFragment>(Entity);

			if (ApproachFragment != nullptr && ChaseParams != nullptr)
			{
				// 티어 속도 배수를 개체별 흔들림 위에 곱한다. 무리 안의 빠르고 느린 차이는
				// 유지하면서 티어가 오를수록 전체가 함께 빨라진다.
				ApproachFragment->SpeedScale = UE::TR::Enemy::MakeSpeedScale(*ChaseParams, RandomStream)
					* FMath::Max(Params.StatScale.Speed, 0.01f);
			}
		}

		// 첫 지면 트레이스 시각을 개체마다 흩어 놓는다.
		//
		// 기본값 0이면 **배치 전원이 스폰 직후 같은 프레임에 트레이스**한다. 한 웨이브가
		// 수십 마리면 그 프레임만 씬 쿼리가 몰려 눈에 띄게 튄다. 프로세서가 트레이스마다
		// 지터를 더하고는 있지만, 그건 두 번째 트레이스부터라 정작 가장 몰리는 스폰
		// 순간에는 아무 효과가 없다.
		//
		// 스폰 난수열을 쓰므로 같은 시드면 같은 결과다(클라이언트 시뮬 동기화 대비).
		if (FTRGroundFragment* GroundFragment = EntityManager.GetFragmentDataPtr<FTRGroundFragment>(Entity))
		{
			const float Interval = (ChaseParams != nullptr) ? ChaseParams->GroundTraceInterval : 0.f;
			if (Interval > 0.f)
			{
				const UWorld* World = EntityManager.GetWorld();
				const float WorldTime = World ? World->GetTimeSeconds() : 0.f;

				GroundFragment->NextTraceTime = WorldTime + RandomStream.FRandRange(0.f, Interval);
			}
		}

		// 스폰 시점에 채널을 덮어써서, 같은 Config로 서로 다른 대상을 쫓는 무리를 만들 수 있다.
		if (FTRChaseChannelFragment* ChannelFragment = EntityManager.GetFragmentDataPtr<FTRChaseChannelFragment>(Entity))
		{
			ChannelFragment->Channel = Params.Channel;
		}

		// 복제 식별자. 이걸 채워야 클라이언트가 이 적을 볼 수 있다.
		if (FTRMassNetIdFragment* NetIdFragment = EntityManager.GetFragmentDataPtr<FTRMassNetIdFragment>(Entity))
		{
			NetIdFragment->NetId = NetId;
			NetIdFragment->ConfigIndex = ConfigIndex;
		}

		// 티어 배수를 여기서 한 번만 적용한다. 이후 로직은 배수를 몰라도 되고,
		// "현재 체력 / 최대 체력" 같은 비교도 같은 기준에서 이뤄진다.
		FTREnemyCombatFragment* CombatFragment = EntityManager.GetFragmentDataPtr<FTREnemyCombatFragment>(Entity);
		const FTREnemyCombatParams* CombatParams =
			EntityManager.GetConstSharedFragmentDataPtr<FTREnemyCombatParams>(Entity);

		if (CombatFragment == nullptr || CombatParams == nullptr)
		{
			// 트레이트가 기준값을 이미 채워두므로, 배수만 못 받은 상태로 정상 동작한다.
			return;
		}

		UE::TR::Enemy::InitCombat(*CombatParams, Params.StatScale, *CombatFragment);

		if (bReportConfigIssues)
		{
			WarnIfSpeedExceedsMovementLimit(EntityManager, Entity, *CombatParams, ChaseParams,
				Params.StatScale, Config);
		}
	}
}

void UTRMassSpawnerSubsystem::Deinitialize()
{
	SpawnedEntities.Reset();

	Super::Deinitialize();
}

int32 UTRMassSpawnerSubsystem::SpawnChasers(const FTRChaserSpawnParams& Params)
{
	TArray<FMassEntityHandle> Unused;
	return SpawnChasersWithHandles(Params, Unused);
}

int32 UTRMassSpawnerSubsystem::SpawnChasersWithHandles(const FTRChaserSpawnParams& Params,
	TArray<FMassEntityHandle>& OutEntities)
{
	OutEntities.Reset();

	UMassEntityConfigAsset* Config = Params.EnemyConfig;
	const int32 Count = Params.Count;

	if (Config == nullptr || Count <= 0)
	{
		return 0;
	}

	UWorld* World = GetWorld();
	UMassSpawnerSubsystem* Spawner = World ? World->GetSubsystem<UMassSpawnerSubsystem>() : nullptr;
	if (Spawner == nullptr)
	{
		UE_LOG(LogTRMass, Warning, TEXT("MassSpawnerSubsystem을 찾을 수 없어 스폰을 건너뜁니다."));
		return 0;
	}

	const FMassEntityTemplate& Template = Config->GetOrCreateEntityTemplate(*World);
	if (!Template.IsValid())
	{
		UE_LOG(LogTRMass, Warning, TEXT("'%s'의 엔티티 템플릿이 유효하지 않습니다. 트레이트 구성을 확인하세요."),
			*GetNameSafe(Config));
		return 0;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	// 호출마다 다른 시드를 쓴다.
	//
	// 프레임 번호만 쓰면 **같은 프레임에 스폰된 그룹끼리 같은 난수열**을 받는다.
	// 웨이브 시작 시 지연 없는 그룹이 둘 이상이면 앞쪽 개체들이 정확히 같은 좌표에
	// 겹쳐 나오고, 최소 간격 검사는 같은 호출 안에서만 비교하므로 이걸 못 잡는다.
	FRandomStream RandomStream(static_cast<int32>(HashCombine(GFrameNumber, ++SpawnSerial)));

	// 스폰 영역 설정을 옮겨 담지 않고 그대로 넘긴다. 필드를 하나씩 복사하던 시절에는
	// 새 옵션을 추가할 때마다 이 자리를 같이 고쳐야 했고, 빠뜨리면 조용히 무시됐다.
	TArray<FVector> Locations;
	UE::TR::MassSpawn::ComputeScatterLocations(World, Params.Area, Count, RandomStream, Locations);

	// 이 배치가 어떤 메시로 표현되는지 클라이언트에 알리기 위한 색인.
	//
	// **스탠드얼론에서도 등록한다.** 이 게임은 맵 이동 없이 방을 열기 때문에
	// (UTRGameInstance::StartListenInPlace) 지금 스탠드얼론인 월드가 곧 서버가 될 수 있다.
	// 리슨 전에 스폰된 개체를 팔레트에 안 넣으면 그 개체들의 ConfigIndex 가 0으로 남아,
	// 나중에 접속한 클라이언트가 **엉뚱한 메시로 그리거나 아예 못 그린다**.
	// 릴레이는 틱이 꺼진 AInfo 하나라 미리 만들어 두는 비용이 사실상 없다.
	uint8 ConfigIndex = 0;
	if (ATRMassNetRelay* Relay = ATRMassNetRelay::Get(World, /*bCreateIfMissing=*/World->GetNetMode() != NM_Client))
	{
		ConfigIndex = Relay->RegisterConfig(Config);
	}

	// 생성 컨텍스트가 살아 있는 동안은 옵저버/명령이 보류된다.
	// 그 사이에 초기값을 다 채워 넣어야 옵저버가 완성된 상태를 보게 된다.
	{
		const TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
			Spawner->SpawnEntities(Template, static_cast<uint32>(Count), OutEntities);

		for (int32 Index = 0; Index < OutEntities.Num(); ++Index)
		{
			InitializeSpawnedEntity(EntityManager, OutEntities[Index], Params,
				Locations.IsValidIndex(Index) ? Locations[Index] : Params.Area.Origin, RandomStream,
				/*bReportConfigIssues=*/Index == 0, Config,
				/*NetId=*/++NetIdSerial, ConfigIndex);
		}
	}

	SpawnedEntities.Append(OutEntities);

	return OutEntities.Num();
}

int32 UTRMassSpawnerSubsystem::DespawnChasers(int32 Count)
{
	CompactSpawnedEntities();

	if (SpawnedEntities.IsEmpty())
	{
		return 0;
	}

	UWorld* World = GetWorld();
	UMassSpawnerSubsystem* Spawner = World ? World->GetSubsystem<UMassSpawnerSubsystem>() : nullptr;
	if (Spawner == nullptr)
	{
		return 0;
	}

	const int32 NumToRemove = (Count <= 0) ? SpawnedEntities.Num() : FMath::Min(Count, SpawnedEntities.Num());

	// 뒤에서부터 잘라내면 배열 이동이 없다.
	TArray<FMassEntityHandle> ToDestroy;
	ToDestroy.Append(SpawnedEntities.GetData() + (SpawnedEntities.Num() - NumToRemove), NumToRemove);
	SpawnedEntities.SetNum(SpawnedEntities.Num() - NumToRemove, EAllowShrinking::No);

	Spawner->DestroyEntities(ToDestroy);

	return NumToRemove;
}

int32 UTRMassSpawnerSubsystem::GetSpawnedCount()
{
	CompactSpawnedEntities();

	const UWorld* World = GetWorld();
	if (World == nullptr || SpawnedEntities.IsEmpty())
	{
		return 0;
	}

	const FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	// 시체는 세지 않는다.
	//
	// 이 값이 곧 웨이브의 동시 상한(성능 예산) 판정에 쓰이는데, 죽은 개체는 DeathLinger
	// 동안 유효한 엔티티로 남는다. 세어 버리면 **플레이어가 많이 죽일수록 다음 물량이
	// 줄어드는** 방향이 된다 — 잘하는 쪽에 난이도를 깎아 주는 셈이라 의도와 정반대다.
	//
	// 판정을 FTRDeadTag가 아니라 행동 상태로 하는 이유는 태그가 명령 버퍼를 거쳐 한 프레임
	// 늦게 붙기 때문이다. 상태는 죽는 즉시 바뀌므로 그 한 프레임의 오차가 없다.
	// 상태 프래그먼트가 없는 구성(TR Chaser 트레이트를 안 쓴 Config)은 살아 있는 것으로 본다.
	int32 AliveCount = 0;
	for (const FMassEntityHandle& Entity : SpawnedEntities)
	{
		const FTREnemyStateFragment* State = EntityManager.GetFragmentDataPtr<FTREnemyStateFragment>(Entity);

		if (State == nullptr || State->ActionState != ETREnemyActionState::Death)
		{
			++AliveCount;
		}
	}

	return AliveCount;
}

void UTRMassSpawnerSubsystem::CompactSpawnedEntities()
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		SpawnedEntities.Reset();
		return;
	}

	const FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	SpawnedEntities.RemoveAll([&EntityManager](const FMassEntityHandle& Entity)
	{
		return !EntityManager.IsEntityValid(Entity);
	});
}

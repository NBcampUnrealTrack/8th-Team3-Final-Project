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
#include "Mass/Spawning/TRMassSpawnUtils.h"

namespace
{
	/**
	 * 단계 속도가 엔진 클램프(Mass Movement의 Max Speed)를 넘는지 검사한다.
	 *
	 * 넘으면 UMassApplyForceProcessor가 잘라내는데, 에러도 경고도 없이 그냥 느리게
	 * 움직일 뿐이라 원인을 찾기 매우 어렵다. Config를 잘못 채운 순간 알려준다.
	 */
	void WarnIfSpeedExceedsMovementLimit(const FMassEntityManager& EntityManager, const FMassEntityHandle& Entity,
		const FTREnemyCombatParams& CombatParams, const UMassEntityConfigAsset* Config)
	{
		const FMassMovementParameters* MovementParams =
			EntityManager.GetConstSharedFragmentDataPtr<FMassMovementParameters>(Entity);

		if (MovementParams == nullptr)
		{
			return;
		}

		// 접근 속도와 사출 속도 중 이 엔티티가 실제로 낼 수 있어야 하는 최댓값.
		float RequiredSpeed = CombatParams.ApproachSpeed;
		if (CombatParams.EnemyType == ETREnemyType::Launcher)
		{
			RequiredSpeed = FMath::Max(RequiredSpeed, CombatParams.LaunchSpeed);
		}

		if (RequiredSpeed > MovementParams->MaxSpeed)
		{
			UE_LOG(LogTRMass, Warning,
				TEXT("'%s'(%s): 필요한 속도 %.0f cm/s가 Mass Movement의 Max Speed(%.0f)를 넘습니다. ")
				TEXT("엔진이 잘라내므로 설정한 속도로 움직이지 않습니다. Max Speed를 %.0f 이상으로 올리세요."),
				*GetNameSafe(Config), LexToString(CombatParams.EnemyType),
				RequiredSpeed, MovementParams->MaxSpeed, RequiredSpeed);
		}
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
		bool bReportConfigIssues, const UMassEntityConfigAsset* Config)
	{
		if (FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
		{
			TransformFragment->GetMutableTransform().SetLocation(Location);
		}

		// 개체별 속도 배수. 이걸 안 뽑으면 한 무리가 통째로 같은 속도로 움직여서
		// 대열이 얼어붙은 것처럼 보인다. 스폰 시 한 번만 정하고 이후 바꾸지 않는다.
		{
			FTRApproachFragment* ApproachFragment = EntityManager.GetFragmentDataPtr<FTRApproachFragment>(Entity);
			const FTRChaseParams* ChaseParams = EntityManager.GetConstSharedFragmentDataPtr<FTRChaseParams>(Entity);

			if (ApproachFragment != nullptr && ChaseParams != nullptr)
			{
				ApproachFragment->SpeedScale = UE::TR::Enemy::MakeSpeedScale(*ChaseParams, RandomStream);
			}
		}

		// 스폰 시점에 채널을 덮어써서, 같은 Config로 서로 다른 대상을 쫓는 무리를 만들 수 있다.
		if (FTRChaseChannelFragment* ChannelFragment = EntityManager.GetFragmentDataPtr<FTRChaseChannelFragment>(Entity))
		{
			ChannelFragment->Channel = Params.Channel;
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
			WarnIfSpeedExceedsMovementLimit(EntityManager, Entity, *CombatParams, Config);
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

	// 생성 컨텍스트가 살아 있는 동안은 옵저버/명령이 보류된다.
	// 그 사이에 초기값을 다 채워 넣어야 옵저버가 완성된 상태를 보게 된다.
	{
		const TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
			Spawner->SpawnEntities(Template, static_cast<uint32>(Count), OutEntities);

		for (int32 Index = 0; Index < OutEntities.Num(); ++Index)
		{
			InitializeSpawnedEntity(EntityManager, OutEntities[Index], Params,
				Locations.IsValidIndex(Index) ? Locations[Index] : Params.Area.Origin, RandomStream,
				/*bReportConfigIssues=*/Index == 0, Config);
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

	return SpawnedEntities.Num();
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

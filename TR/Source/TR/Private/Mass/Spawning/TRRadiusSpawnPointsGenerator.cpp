// TRRadiusSpawnPointsGenerator.cpp

#include "Mass/Spawning/TRRadiusSpawnPointsGenerator.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MassSpawnLocationProcessor.h"
#include "MassSpawnerTypes.h"
#include "Mass/Spawning/TRMassSpawnUtils.h"

void UTRRadiusSpawnPointsGenerator::Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes,
	int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
	if (Count <= 0)
	{
		FinishedGeneratingSpawnPointsDelegate.Execute(TArray<FMassEntitySpawnDataGeneratorResult>());
		return;
	}

	// QueryOwner는 이 Generator를 소유한 MassSpawner 액터다. 그 위치를 중심으로 삼는다.
	const AActor* OwnerActor = Cast<AActor>(&QueryOwner);
	const FTransform OwnerTransform = OwnerActor ? OwnerActor->GetActorTransform() : FTransform::Identity;
	const UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;

	// EntityTypes의 Proportion에 따라 개수를 나눠 담은 빈 결과를 만들어준다.
	TArray<FMassEntitySpawnDataGeneratorResult> Results;
	BuildResultsFromEntityTypes(Count, EntityTypes, Results);

	FRandomStream RandomStream(GetRandomSelectionSeed());

	// 기준점과 방위만 액터에서 채우고, 모양 설정은 디테일 패널 값을 그대로 쓴다.
	FTRSpawnAreaParams AreaParams = Area;
	AreaParams.Origin = OwnerTransform.GetLocation();
	AreaParams.OrientationYawDegrees = OwnerTransform.Rotator().Yaw;
	AreaParams.IgnoredActor = OwnerActor;

	TArray<FVector> Locations;

	for (FMassEntitySpawnDataGeneratorResult& Result : Results)
	{
		// 위치 데이터를 엔티티에 실제로 적용하는 건 엔진의 이 프로세서다.
		Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
		Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();

		FMassTransformsSpawnData& SpawnData = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();
		SpawnData.Transforms.Reserve(Result.NumEntities);

		// 최소 간격 검사는 같은 배치 안에서만 의미가 있으므로 한 번에 만든다.
		UE::TR::MassSpawn::ComputeScatterLocations(World, AreaParams, Result.NumEntities, RandomStream, Locations);

		for (const FVector& Location : Locations)
		{
			const FQuat Rotation = bRandomizeYaw
				? FRotator(0.f, RandomStream.FRandRange(0.f, 360.f), 0.f).Quaternion()
				: OwnerTransform.GetRotation();

			SpawnData.Transforms.Emplace(Rotation, Location, FVector::OneVector);
		}
	}

	FinishedGeneratingSpawnPointsDelegate.Execute(Results);
}

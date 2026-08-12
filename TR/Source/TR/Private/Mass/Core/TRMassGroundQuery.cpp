// TRMassGroundQuery.cpp

#include "Mass/Core/TRMassGroundQuery.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace UE::TR::MassGround
{
	bool FindLowestGroundZ(const UWorld& World, const FVector& Location,
		float TraceUpOffset, float TraceDownOffset, const AActor* IgnoredActor, float& OutGroundZ)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TRMassGroundLowest), /*bTraceComplex=*/false);
		if (IgnoredActor != nullptr)
		{
			QueryParams.AddIgnoredActor(IgnoredActor);
		}

		TArray<FHitResult> Hits;
		if (!World.LineTraceMultiByChannel(Hits,
			Location + FVector(0.f, 0.f, TraceUpOffset),
			Location - FVector(0.f, 0.f, TraceDownOffset),
			ECC_WorldStatic, QueryParams))
		{
			return false;
		}

		bool bFound = false;
		double LowestZ = 0.0;

		for (const FHitResult& Hit : Hits)
		{
			if (Hit.ImpactNormal.Z < MinWalkableNormalZ)
			{
				// 벽이나 급경사. 여기 세우면 끼어버린다.
				continue;
			}

			if (!bFound || Hit.ImpactPoint.Z < LowestZ)
			{
				LowestZ = Hit.ImpactPoint.Z;
				bFound = true;
			}
		}

		if (bFound)
		{
			OutGroundZ = static_cast<float>(LowestZ);
		}

		return bFound;
	}

	bool FindGroundZBelow(const UWorld& World, const FVector& Location,
		float TraceUpOffset, float TraceDownOffset, float& OutGroundZ)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TRMassGroundBelow), /*bTraceComplex=*/false);

		FHitResult Hit;
		if (!World.LineTraceSingleByChannel(Hit,
			Location + FVector(0.f, 0.f, TraceUpOffset),
			Location - FVector(0.f, 0.f, TraceDownOffset),
			ECC_WorldStatic, QueryParams))
		{
			return false;
		}

		if (Hit.ImpactNormal.Z < MinWalkableNormalZ)
		{
			return false;
		}

		OutGroundZ = static_cast<float>(Hit.ImpactPoint.Z);
		return true;
	}
}

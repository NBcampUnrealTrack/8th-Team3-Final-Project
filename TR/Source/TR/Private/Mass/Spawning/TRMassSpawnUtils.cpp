// TRMassSpawnUtils.cpp

#include "Mass/Spawning/TRMassSpawnUtils.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Core/TRMassGroundQuery.h"

namespace UE::TR::MassSpawn
{
	namespace Private
	{
		/** 지면 탐색 트레이스의 위/아래 여유. 스폰 지점은 지형을 모르므로 넉넉히 잡는다. */
		constexpr float TraceUpOffset = 1000.f;
		constexpr float TraceDownOffset = 10000.f;

		/** 최소 간격을 지키는 자리를 찾기 위한 시도 횟수. 넘으면 그냥 배치한다. */
		constexpr int32 MaxPlacementAttempts = 12;

		/**
		 * [0,1) 구간을 Count개로 나눈 Index번째 대표값. 균등 배치의 기본 단위다.
		 * 칸의 가운데를 쓰므로 양 끝이 경계에 딱 붙지 않는다.
		 */
		float EvenAlpha(int32 Index, int32 Count)
		{
			return (Count > 1) ? ((static_cast<float>(Index) + 0.5f) / static_cast<float>(Count)) : 0.5f;
		}

		/** 고리 — 중심에서 Inner~Outer 거리, 지정된 호 안. 각도는 로컬(0 = 방위 방향). */
		FVector2D PointInAnnulus(const FTRSpawnAreaParams& Params, int32 Index, int32 Count, FRandomStream& RandomStream)
		{
			if (Params.OuterRadius <= 0.f)
			{
				return FVector2D::ZeroVector;
			}

			const float RangeDegrees = FMath::Clamp(Params.AngleRangeDegrees, 0.f, 360.f);

			// 균등이면 호를 등분한다. 무작위면 호 안에서 아무 각도.
			const float Alpha = Params.ShapeParams.bEvenlySpaced ? EvenAlpha(Index, Count) : RandomStream.FRand();
			const float Angle = FMath::DegreesToRadians(Params.AngleStartDegrees + RangeDegrees * Alpha);

			// 거리는 균등 배치에서도 무작위로 둔다. 전부 같은 거리면 자로 그은 원이 되어
			// 기계처럼 보이고, 앞줄이 뒷줄을 가려 물량이 적어 보인다.
			//
			// 제곱근을 씌워야 고리 안에 고르게 퍼진다. 균등 난수를 그대로 쓰면 안쪽으로 몰린다.
			const float ClampedInner = FMath::Clamp(Params.InnerRadius, 0.f, Params.OuterRadius);
			const float NormalizedRadius = FMath::Sqrt(RandomStream.FRand());
			const float Distance = ClampedInner + (Params.OuterRadius - ClampedInner) * NormalizedRadius;

			return FVector2D(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance);
		}

		/** 사각 — 방위 방향으로 ShapeOffset만큼 떨어진 자리의 직사각형. 균등이면 격자. */
		FVector2D PointInBox(const FTRSpawnAreaParams& Params, int32 Index, int32 Count, FRandomStream& RandomStream)
		{
			const FVector2D Extent = Params.ShapeParams.BoxExtent.GetAbs();

			float LocalX;
			float LocalY;

			if (Params.ShapeParams.bEvenlySpaced)
			{
				// 가로세로 비율을 유지한 격자로 나눈다. 폭이 넓으면 열이 많아진다.
				const float AspectRatio = (Extent.X > KINDA_SMALL_NUMBER) ? (Extent.Y / Extent.X) : 1.f;
				const int32 Columns = FMath::Max(1, FMath::RoundToInt(FMath::Sqrt(Count * AspectRatio)));
				const int32 Rows = FMath::DivideAndRoundUp(Count, Columns);

				const int32 Column = Index % Columns;
				const int32 Row = Index / Columns;

				LocalX = FMath::Lerp(-Extent.X, Extent.X, EvenAlpha(Row, Rows));
				LocalY = FMath::Lerp(-Extent.Y, Extent.Y, EvenAlpha(Column, Columns));
			}
			else
			{
				LocalX = RandomStream.FRandRange(-Extent.X, Extent.X);
				LocalY = RandomStream.FRandRange(-Extent.Y, Extent.Y);
			}

			return FVector2D(Params.ShapeOffset + LocalX, LocalY);
		}

		/** 선분 — 방위와 직각으로 늘어선 한 줄. 두께만큼 앞뒤 흔들림을 준다. */
		FVector2D PointOnLine(const FTRSpawnAreaParams& Params, int32 Index, int32 Count, FRandomStream& RandomStream)
		{
			const float HalfLength = FMath::Abs(Params.ShapeParams.LineLength) * 0.5f;
			const float HalfThickness = FMath::Abs(Params.ShapeParams.LineThickness) * 0.5f;

			const float Alpha = Params.ShapeParams.bEvenlySpaced ? EvenAlpha(Index, Count) : RandomStream.FRand();

			const float LocalY = FMath::Lerp(-HalfLength, HalfLength, Alpha);
			const float LocalX = RandomStream.FRandRange(-HalfThickness, HalfThickness);

			return FVector2D(Params.ShapeOffset + LocalX, LocalY);
		}

		/**
		 * 모양이 정한 로컬 좌표를 월드 위치로 바꾼다. 높이는 건드리지 않는다.
		 *
		 * 회전을 여기 한 곳에서만 하므로, 모양 함수들은 방위를 몰라도 된다.
		 */
		FVector ComputePoint(const FTRSpawnAreaParams& Params, int32 Index, int32 Count, FRandomStream& RandomStream)
		{
			float DirectionOffset = Params.DirectionOffsetDegrees;

			// 좌우 분할. 모양 함수에는 "이쪽 편에서 몇 번째인가"를 넘겨야
			// 균등 배치가 양쪽 각각에서 등간격이 된다. 전체 인덱스를 그대로 주면
			// 한쪽은 앞쪽에만, 다른 쪽은 뒤쪽에만 몰린다.
			int32 ShapeIndex = Index;
			int32 ShapeCount = Count;

			if (Params.bMirrorDirection)
			{
				const bool bSecondSide = ((Index % 2) != 0);
				if (bSecondSide)
				{
					DirectionOffset = -DirectionOffset;
				}

				ShapeIndex = Index / 2;
				ShapeCount = FMath::Max(1, bSecondSide ? (Count / 2) : ((Count + 1) / 2));
			}

			FVector2D Local;
			switch (Params.ShapeParams.Shape)
			{
			case ETRSpawnShape::Box:  Local = PointInBox(Params, ShapeIndex, ShapeCount, RandomStream);   break;
			case ETRSpawnShape::Line: Local = PointOnLine(Params, ShapeIndex, ShapeCount, RandomStream);  break;
			default:                  Local = PointInAnnulus(Params, ShapeIndex, ShapeCount, RandomStream); break;
			}

			const FVector Rotated = FRotator(0.f, Params.OrientationYawDegrees + DirectionOffset, 0.f)
				.RotateVector(FVector(Local.X, Local.Y, 0.f));

			return Params.Origin + Rotated;
		}

		/** 가장 아래쪽의 걸을 수 있는 면으로 높이를 맞춘다. 판정 자체는 공용 함수가 한다. */
		bool SnapToGround(const UWorld& World, const AActor* IgnoredActor, FVector& InOutLocation)
		{
			float GroundZ = 0.f;
			if (!UE::TR::MassGround::FindLowestGroundZ(World, InOutLocation,
				TraceUpOffset, TraceDownOffset, IgnoredActor, GroundZ))
			{
				return false;
			}

			InOutLocation.Z = GroundZ;
			return true;
		}
	}

	void ComputeScatterLocations(const UWorld* World, const FTRSpawnAreaParams& Params,
		int32 Count, FRandomStream& RandomStream, TArray<FVector>& OutLocations)
	{
		OutLocations.Reset();
		if (Count <= 0)
		{
			return;
		}

		OutLocations.Reserve(Count);

		// 균등 배치는 자리가 이미 결정되어 있다. 재시도해도 같은 점이 나오므로 검사를 건너뛴다.
		const bool bCheckSeparation = (Params.ShapeParams.MinSeparation > 0.f) && !Params.ShapeParams.bEvenlySpaced;
		const float MinSeparationSq = FMath::Square(Params.ShapeParams.MinSeparation);

		// 지면을 못 찾은 횟수. 스폰 지점에 바닥이 없으면 높이가 Origin 그대로 남아
		// 공중에 뜬 것처럼 보이는데, 조용히 넘어가면 원인을 찾기 어렵다.
		int32 GroundMissCount = 0;

		for (int32 Index = 0; Index < Count; ++Index)
		{
			FVector Accepted = FVector::ZeroVector;
			bool bAccepted = false;

			// 최소 간격을 만족하는 자리를 몇 번 찾아보고, 못 찾으면 마지막 후보를 그냥 쓴다.
			// 반경이 좁은데 개체가 많으면 애초에 불가능하므로 무한 루프를 만들면 안 된다.
			const int32 AttemptCount = bCheckSeparation ? Private::MaxPlacementAttempts : 1;

			for (int32 Attempt = 0; Attempt < AttemptCount; ++Attempt)
			{
				FVector Candidate = Private::ComputePoint(Params, Index, Count, RandomStream);

				if (Params.bSnapToGround && World != nullptr)
				{
					if (!Private::SnapToGround(*World, Params.IgnoredActor, Candidate))
					{
						++GroundMissCount;
					}
				}

				Accepted = Candidate;

				if (!bCheckSeparation)
				{
					bAccepted = true;
					break;
				}

				bool bTooClose = false;
				for (const FVector& Existing : OutLocations)
				{
					if (FVector::DistSquared2D(Existing, Candidate) < MinSeparationSq)
					{
						bTooClose = true;
						break;
					}
				}

				if (!bTooClose)
				{
					bAccepted = true;
					break;
				}
			}

			// bAccepted가 false여도 마지막 후보를 넣는다. 요청한 수는 맞춰줘야 한다.
			OutLocations.Add(Accepted);
		}

		if (GroundMissCount > 0)
		{
			UE_LOG(LogTRMass, Warning,
				TEXT("스폰 지면 탐색 실패 %d/%d회. 해당 개체는 Origin 높이(Z=%.0f)에 그대로 배치됩니다. ")
				TEXT("스폰 영역(모양 %d, 방위 %.0f도) 안에 바닥 콜리전이 있는지 확인하세요."),
				GroundMissCount, Count, Params.Origin.Z,
				static_cast<int32>(Params.ShapeParams.Shape), Params.OrientationYawDegrees);
		}
	}
}

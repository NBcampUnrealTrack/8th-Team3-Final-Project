// TRMassSpawnUtils.h

#pragma once

#include "CoreMinimal.h"
#include "Mass/Core/TRMassTypes.h"
#include "TRMassSpawnUtils.generated.h"

class UWorld;

/**
 * 모양 설정 — **사람이 정하는 값**만 모았다.
 *
 * 스폰 위치는 두 종류의 값으로 결정된다.
 *   ① 사람이 정하는 것 — 어떤 모양인가, 얼마나 큰가          ← 이 구조체
 *   ② 상황이 정하는 것 — 어디에, 어느 방향으로, 얼마나 멀리   ← FTRSpawnAreaParams의 나머지
 *
 * 둘을 나눠 둔 이유는 웨이브 테이블 때문이다. 웨이브는 ②를 "방위 + 스폰 거리"라는
 * 다른 어휘로 적고 런타임에 환산하는데, ①은 그대로 전달만 하면 된다. 예전에는
 * 웨이브 행이 같은 필드를 따로 들고 한 줄씩 옮겨 담았고, 새 모양 옵션을 추가할 때
 * 그 복사 목록에 넣는 걸 잊으면 웨이브에서만 조용히 무시됐다.
 */
USTRUCT(BlueprintType)
struct FTRSpawnShapeParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	ETRSpawnShape Shape = ETRSpawnShape::Annulus;

	/**
	 * 무작위 대신 균등 배치.
	 *
	 * 고리는 호를 등분한 각도, 선은 등간격, 사각은 격자가 된다.
	 * 무리처럼 보이려면 끄고, 부대 진형처럼 보이려면 켠다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	bool bEvenlySpaced = false;

	/**
	 * 개체 사이 최소 간격. 스폰 순간의 겹침을 막는다.
	 * 0이면 검사하지 않는다. 영역 대비 너무 크면 자리를 못 찾아 그냥 배치된다.
	 * (bEvenlySpaced가 켜져 있으면 배치가 이미 결정적이라 이 검사를 건너뛴다.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MinSeparation = 120.f;

	/** 사각의 반크기. X = 방위 방향 깊이, Y = 좌우 폭. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape",
		meta = (ForceUnits = "cm", EditCondition = "Shape == ETRSpawnShape::Box", EditConditionHides))
	FVector2D BoxExtent = FVector2D(500.f, 2000.f);

	/** 선분의 길이(방위와 직각 방향). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape",
		meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "Shape == ETRSpawnShape::Line", EditConditionHides))
	float LineLength = 4000.f;

	/** 선분의 두께(방위 방향). 완전히 일직선이면 부자연스러우므로 약간 준다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape",
		meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "Shape == ETRSpawnShape::Line", EditConditionHides))
	float LineThickness = 200.f;
};

/**
 * 스폰 영역 설정 — "어디에, 어떤 모양으로 흩뿌릴 것인가".
 *
 * 레벨 배치용 Generator와 런타임 웨이브 스포너가 **같은 구조체를 그대로 넘긴다.**
 * 예전에는 양쪽이 각자 같은 필드를 들고 있다가 옮겨 담았는데, 한쪽에만 옵션을
 * 추가하면 다른 경로에서 조용히 무시되는 문제가 있었다.
 *
 * 좌표계 — 모양은 전부 **OrientationYawDegrees가 가리키는 방향을 로컬 +X**로 보고 정의된다.
 * 호출부가 "기차 진행 방향 + 방위 오프셋"을 이 한 값에 넣어주면, 각 모양은 월드 각도를
 * 몰라도 된다. 방위(후/좌/우)를 바꿔도 모양 설정은 그대로 쓸 수 있다.
 */
USTRUCT(BlueprintType)
struct FTRSpawnAreaParams
{
	GENERATED_BODY()

	/** 기준점. 보통 기차 위치. 런타임에 채운다. */
	UPROPERTY(BlueprintReadWrite, Category = "Area")
	FVector Origin = FVector::ZeroVector;

	/** 기준 축(월드 Yaw). 보통 기차 진행 방향. 런타임에 채운다. */
	UPROPERTY(BlueprintReadWrite, Category = "Area")
	float OrientationYawDegrees = 0.f;

	/**
	 * 기준 축에서 방위만큼 돌린 각도. 후=180, 좌=90, 우=-90.
	 * 최종 방향 = OrientationYaw + 이 값이고, 모양의 로컬 +X가 그쪽을 향한다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Area")
	float DirectionOffsetDegrees = 0.f;

	/**
	 * 방위를 좌우 대칭으로 갈라 절반씩 배치한다(협공 패턴).
	 *
	 * 홀수 번째 개체가 -DirectionOffset 쪽으로 간다. 모양과 무관하게 동작하므로
	 * 사각·선도 그대로 양쪽에 하나씩 놓인다. 균등 배치와 함께 쓰면 각 방향이
	 * 자기 안에서 등간격이 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	bool bMirrorDirection = false;

	/** 모양과 크기. 웨이브 테이블은 이 부분만 그대로 넘긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	FTRSpawnShapeParams ShapeParams;

	/** 아래로 트레이스해서 지면 높이에 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	bool bSnapToGround = true;

	// ── Annulus (고리) ────────────────────────────────────────────────────

	/** 비워둘 안쪽 반경. 기차 위에 얹히는 걸 막는 1차 방어선이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Annulus",
		meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "Shape == ETRSpawnShape::Annulus", EditConditionHides))
	float InnerRadius = 0.f;

	/** 바깥 반경. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Annulus",
		meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "Shape == ETRSpawnShape::Annulus", EditConditionHides))
	float OuterRadius = 1000.f;

	/**
	 * 허용되는 호(arc). **OrientationYawDegrees 기준 상대 각도**다.
	 * 기획서의 90도 구역이면 Start = -45, Range = 90. 전방위면 Range = 360.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Annulus")
	float AngleStartDegrees = -180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Annulus", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float AngleRangeDegrees = 360.f;

	/**
	 * 사각·선의 중심을 기준점에서 로컬 +X로 얼마나 밀어낼지.
	 * 고리의 반경에 해당하는 값 — 기획서의 "기차에서 200m 밖"이 여기 들어간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ShapeOffset = 0.f;

	/**
	 * 지면 트레이스에서 제외할 액터(보통 스포너 자신).
	 *
	 * UPROPERTY가 아니다 — 호출 한 번 동안만 유효한 값이라 보관하지 않는다.
	 * 이 구조체를 프레임 너머로 들고 있으면 안 된다.
	 */
	const AActor* IgnoredActor = nullptr;
};

namespace UE::TR::MassSpawn
{
	/**
	 * Count개의 스폰 위치를 만든다.
	 *
	 * 지면 스냅은 "가장 아래쪽의 걸을 수 있는 면"을 고른다. 첫 충돌을 쓰면 기차 지붕이나
	 * 구조물 위에 올라앉아 적이 머리 위에서 튀어나온 것처럼 보이기 때문이다.
	 * (지면 위에 다리 같은 구조물이 있는 맵이라면 이 규칙을 다시 봐야 한다.)
	 */
	TR_API void ComputeScatterLocations(const UWorld* World, const FTRSpawnAreaParams& Params,
		int32 Count, FRandomStream& RandomStream, TArray<FVector>& OutLocations);
}

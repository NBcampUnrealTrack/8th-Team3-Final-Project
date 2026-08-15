// TRMassParams.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/Core/TRMassFragments.h"
#include "TRMassParams.generated.h"


/** 추적·회전·분리 파라미터. 종류와 무관하게 공통이다. */
USTRUCT(BlueprintType)
struct FTRChaseParams : public FMassConstSharedFragment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Chase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedRatio = 1.f;

	/**
	 * 회전이 목표 방향을 따라잡는 데 걸리는 시간(초). 0이면 즉시 회전.
	 *
	 * 각속도 상수(초당 몇 도)가 아니라 시간 상수를 쓴다. 지수 감쇠라 큰 각도는 빠르게,
	 * 목표에 가까워질수록 부드럽게 감속해서 기계적으로 보이지 않는다. 튜닝도 직관적이다.
	 */
	UPROPERTY(EditAnywhere, Category = "Orientation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float OrientationSmoothingTime = 0.25f;

	/**
	 * 이동 중 어느 쪽을 보고 돌지 결정하는 가중치.
	 *
	 * 속도 방향만 보면 회피/분리로 밀릴 때마다 몸이 흔들리고, 목표 방향만 보면 게처럼
	 * 옆으로 걷는다. 둘을 섞는 게 실무 표준이다.
	 */
	UPROPERTY(EditAnywhere, Category = "Orientation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MovingTargetWeight = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Orientation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MovingVelocityWeight = 0.6f;

	/**
	 * 도달했거나 목표 코앞일 때의 가중치.
	 * 멈추면 속도가 0이라 방향이 없으므로 목표 쪽을 봐야 한다. 이 구분이 없으면
	 * 도착하는 순간 회전이 튄다.
	 */
	UPROPERTY(EditAnywhere, Category = "Orientation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StandingTargetWeight = 0.95f;

	UPROPERTY(EditAnywhere, Category = "Orientation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StandingVelocityWeight = 0.05f;

	/**
	 * 도착 몇 초 전부터 정지 가중치로 서서히 넘어갈지.
	 * 거리로 환산해서 쓰므로 빠른 개체일수록 더 멀리서 전환이 시작된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Orientation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float OrientationFadeTime = 1.f;

	/** 이동하면서 지면 높이를 따라간다. 끄면 스폰 당시 높이를 계속 유지한다. */
	UPROPERTY(EditAnywhere, Category = "Ground")
	bool bFollowGround = true;

	/**
	 * 지면 트레이스 주기(초). 매 프레임 트레이스하면 개체 수에 비례해 비싸진다.
	 * 주기를 두고 그 사이는 보간으로 메운다.
	 */
	UPROPERTY(EditAnywhere, Category = "Ground", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float GroundTraceInterval = 0.25f;

	/** 지면 높이를 따라잡는 시간(초). 계단이나 경사에서 튀지 않게 한다. */
	UPROPERTY(EditAnywhere, Category = "Ground", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float GroundSmoothingTime = 0.1f;

	/**
	 * 이웃을 밀어내는 힘(cm/s^2). 0이면 분리 없음.
	 * 목표가 한 점이면 전원이 그리로 수렴해 한 덩어리로 보이는데, 이 힘이 그걸 푼다.
	 */
	UPROPERTY(EditAnywhere, Category = "Separation", meta = (ClampMin = "0.0", ForceUnits = "cm/s^2"))
	float SeparationStrength = 900.f;

	/**
	 * 분리 판정 거리 = (내 반지름 + 이웃 반지름) * 이 값.
	 * 1.0이면 서로 닿을 때만 밀어내므로 이미 겹친 뒤에 반응한다. 조금 크게 잡는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Separation", meta = (ClampMin = "1.0"))
	float SeparationRadiusScale = 1.8f;

	/**
	 * 한 개체가 고려할 이웃 수 상한. 밀집 구간에서 비용이 폭발하는 걸 막는다.
	 * 가까운 순으로 정렬하지 않고 그리드가 주는 순서대로 자르므로, 너무 작으면 방향이 편향된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Separation", meta = (ClampMin = "1"))
	int32 MaxSeparationNeighbors = 8;
};

/**
 * 종류별 전투 수치. Config 애셋마다 이 값만 바꾸면 근접형/사출형/사격형이 갈린다.
 *
 * 여기 있는 값은 **T1(기준) 수치**다. 티어별 수치는 스폰 시 배수(StatScale)를 곱해
 * 개체별 FTREnemyCombatFragment에 넣는다. 기획서의 "웨이브당 +5% 복리"가 그 배수다.
 */
USTRUCT(BlueprintType)
struct FTREnemyCombatParams : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Enemy")
	ETREnemyType EnemyType = ETREnemyType::Melee;

	UPROPERTY(EditAnywhere, Category = "Enemy", meta = (ClampMin = "1.0"))
	float MaxHealth = 25.f;
	
	// 리턴 보상
	UPROPERTY(EditAnywhere, Category = "Enemy", meta = (ClampMin = "0"))
	int32 ResourceValue = 1;

	/**
	 * 목표로 접근할 때의 속도(cm/s). **0이면 Mass Movement의 Max Speed × SpeedRatio를 쓴다.**
	 *
	 * 대부분의 적은 0으로 두고 Mass Movement 쪽에서 속도를 정하면 된다. 값이 필요한 경우는
	 * **한 엔티티가 단계마다 다른 속도를 내는 사출형**뿐이다.
	 *
	 * 엔진이 최종 속도를 FMassMovementParameters::MaxSpeed로 클램프하므로, 사출형은
	 * Mass Movement의 Max Speed를 그 엔티티가 낼 수 있는 **최댓값(= LaunchSpeed)**으로
	 * 잡아야 한다. 그러면 Max Speed는 천장 역할만 하고, 실제 접근 속도는 이 값이 정한다.
	 *
	 *   예) 사출형 T3 — Max Speed = 2500, ApproachSpeed = 1000, LaunchSpeed = 2500
	 *       → 10 m/s로 접근하다가 사출 후 25 m/s로 날아간다.
	 */
	UPROPERTY(EditAnywhere, Category = "Enemy", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float ApproachSpeed = 0.f;

	// ── 접촉 폭발 (근접형 / 사출된 상체) ──────────────────────────────────

	//피해량
	UPROPERTY(EditAnywhere, Category = "Enemy|Detonate", meta = (ClampMin = "0.0"))
	float ContactDamage = 15.f;
	
	UPROPERTY(EditAnywhere, Category = "Enemy|Detonate", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ContactRadius = 300.f;
	
	//폭발시간
	UPROPERTY(EditAnywhere, Category = "Enemy|Detonate", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DetonationWindup = 0.4f;

	// ── 사출형 ────────────────────────────────────────────────────────────

	/** 상체를 사출하는 거리(표면 기준). 기획서 40m. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Launcher", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float LaunchDistance = 4000.f;

	/** 사출된 상체의 비행 속도. 기획서 25 m/s. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Launcher", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float LaunchSpeed = 2500.f;

	/** 사출된 상체의 체력. 요격 난이도를 정하는 값이다. 기획서 10. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Launcher", meta = (ClampMin = "1.0"))
	float LaunchedHealth = 10.f;

	// ── 사격형 ────────────────────────────────────────────────────────────

	// 사격 유지 거리
	UPROPERTY(EditAnywhere, Category = "Enemy|Shooter", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float StandoffDistance = 5500.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Shooter", meta = (ClampMin = "0.0"))
	float ShotDamage = 25.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Shooter", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float FireInterval = 2.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Shooter", meta = (ClampMin = "1"))
	int32 ShotCount = 3;
};

/**
 * 파라미터(설정) → 개체 상태(프래그먼트) 변환.
 *
 * 이 규칙을 호출부마다 다시 쓰지 않는다. 스폰 시점과 사출 시점이 같은 함수를 거쳐야
 * 두 경로의 결과가 어긋나지 않는다.
 */
namespace UE::TR::Enemy
{
	/**
	 * 도달 판정을 푸는 거리에 주는 여유(cm).
	 * 정지 거리와 같으면 경계에서 도달 태그가 매 틱 붙었다 떨어진다(히스테리시스).
	 */
	inline constexpr float LeaveDistanceMargin = 50.f;

	/** 사격 착탄의 판정 반경(cm). 폭발이 아니므로 맞은 자리만 친다. */
	inline constexpr float ShotImpactRadius = 50.f;

	/** 스폰 직후의 접근 수치. 종류가 정지 거리를 정한다. */
	inline FTRApproachFragment MakeApproach(const FTREnemyCombatParams& Params)
	{
		FTRApproachFragment Approach;

		switch (Params.EnemyType)
		{
		case ETREnemyType::Launcher: Approach.StopDistance = Params.LaunchDistance;    break;
		case ETREnemyType::Shooter:  Approach.StopDistance = Params.StandoffDistance;  break;
		default:                     Approach.StopDistance = Params.ContactRadius;     break;
		}

		Approach.LeaveDistance = Approach.StopDistance + LeaveDistanceMargin;

		// 사격형만 표적을 고정한다(기획서). 나머지는 매 틱 가장 가까운 구역으로 붙는다.
		Approach.bLockTargetZone = (Params.EnemyType == ETREnemyType::Shooter);

		// 0이면 Mass Movement의 Max Speed를 그대로 쓴다. 종류별 이동 속도는 그쪽에서
		// 주는 것이 엔진 회피·이동과 일관되므로, 대부분의 적은 여기가 0이다.
		// 값이 들어오는 건 단계마다 속도가 달라지는 사출형뿐이다.
		Approach.MaxSpeed = Params.ApproachSpeed;

		return Approach;
	}

	/** 스폰 직후의 전투 수치. StatScale은 티어 배수(기획서의 웨이브당 +5% 복리). */
	inline void InitCombat(const FTREnemyCombatParams& Params, float StatScale, FTREnemyCombatFragment& OutCombat)
	{
		const float Scale = (StatScale > 0.f) ? StatScale : 1.f;

		OutCombat.MaxHealth = Params.MaxHealth * Scale;
		OutCombat.Health = OutCombat.MaxHealth;
		OutCombat.ContactDamage = Params.ContactDamage * Scale;
		OutCombat.ShotDamage = Params.ShotDamage * Scale;

		// 자원은 밸런스상 물가에 해당한다. 물량과 함께 복리로 불리면 후반에 폭주하므로
		// 배수를 곱하지 않는다(기획서도 T4+ 자원은 고정값이다).
		OutCombat.ResourceValue = Params.ResourceValue;

		OutCombat.NextActionTime = 0.f;
		OutCombat.ShotsFired = 0;
	}

	/**
	 * 사출형이 상체를 쏘아 보낸 순간. 같은 엔티티가 빠르고 무른 투사체가 된다.
	 *
	 * 체력을 상한으로만 낮춘다(Min). 이미 요격당해 더 낮아진 체력을 되돌리면
	 * "쏘는 순간 회복"이 되어 요격이 무의미해진다.
	 */
	inline void ApplyLaunched(const FTREnemyCombatParams& Params,
		FTREnemyCombatFragment& Combat, FTRApproachFragment& Approach)
	{
		Approach.StopDistance = Params.ContactRadius;
		Approach.LeaveDistance = Params.ContactRadius + LeaveDistanceMargin;
		Approach.MaxSpeed = Params.LaunchSpeed;
		Approach.bLockTargetZone = false;

		Combat.MaxHealth = Params.LaunchedHealth;
		Combat.Health = FMath::Min(Combat.Health, Params.LaunchedHealth);

		// 사출된 상체를 요격해도 자원은 나오지 않는다(기획서).
		Combat.ResourceValue = 0;
	}
}

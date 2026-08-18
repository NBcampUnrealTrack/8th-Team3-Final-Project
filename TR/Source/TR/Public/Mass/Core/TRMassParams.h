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
	 * 개체마다 속도를 얼마나 흩을지. 0이면 전부 같은 속도(예전 동작).
	 *
	 * **아래로만 흩는다.** 0.2면 각 개체가 기준 속도의 80~100% 중 하나를 받는다.
	 * 위로 흩지 않는 이유는 엔진이 최종 속도를 FMassMovementParameters::MaxSpeed로
	 * 자르기 때문이다 — SpeedRatio가 1이면 기준 속도가 이미 그 천장이라, 더 빠른
	 * 개체를 만들어도 조용히 잘려서 변주가 한쪽만 먹는다.
	 *
	 * 그래서 **Max Speed는 "가장 빠른 개체의 속도"**가 되고, 무리의 평균은
	 * 그보다 (Variance / 2)만큼 느려진다. 0.2면 평균 90%. 접근 시간을 유지하려면
	 * 그만큼 Max Speed를 올려 잡아야 한다.
	 *
	 * 위로도 흩고 싶으면 Max Speed에 여유를 두고 SpeedRatio를 1 미만으로 내리면 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Chase", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float SpeedVariance = 0.2f;

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
	
	/** 폭발이 닿는 반경. 표적을 고르는 거리가 아니라 터진 자리에서 퍼지는 범위다. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Detonate", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ContactRadius = 300.f;

	/**
	 * 표적 표면까지 이만큼 붙어야 자폭을 시작한다. 폭발 반경과 따로 잡는다.
	 * 거리는 개체 중심 기준이라 메시 앞끝이 표적에 닿는 값으로 맞춘다.
	 */
	UPROPERTY(EditAnywhere, Category = "Enemy|Detonate", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ContactStopDistance = 130.f;

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

	/**
	 * 죽은 뒤 시체가 남아 있는 시간(초). 사망 연출 길이만큼 준다.
	 *
	 * 이 시간 동안 엔티티는 살아 있되 조종·분리에서 제외되어 그 자리에 멈춘다(FTRDeadTag).
	 * 0이면 다음 틱에 바로 사라진다.
	 */
	UPROPERTY(EditAnywhere, Category = "Enemy", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DeathLinger = 2.f;

	/**
	 * 피격 판정용 몸통 높이(cm).
	 *
	 * 엔티티 트랜스폼은 발밑이라, 이 값이 없으면 적이 발 위치의 점 하나가 된다. 그러면
	 * 몸통을 정확히 조준한 사격이 오히려 빗나간다 — 광선이 발밑까지 내려오지 않으니까.
	 * 발밑에서 위로 이만큼을 몸통 선분으로 보고 판정한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Enemy", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float HitHeight = 180.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Shooter", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float FireInterval = 2.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Shooter", meta = (ClampMin = "1"))
	int32 ShotCount = 3;
};

/**
 * SequenceList 배열의 한 칸을 가리키는 참조.
 *
 * UAnimSequenceTransformProviderSequenceList는 이름 없는 배열이라 **순서가 곧 ID**다.
 * 에디터에서 배열 순서를 바꾸면 여기 인덱스도 같이 고쳐야 한다.
 */
USTRUCT(BlueprintType)
struct FTRAnimSequenceRef
{
	GENERATED_BODY()

	FTRAnimSequenceRef() = default;
	explicit FTRAnimSequenceRef(int32 InIndex, float InDuration = 0.f)
		: SequenceIndex(InIndex), Duration(InDuration) {}

	/** SequenceList의 배열 인덱스. */
	UPROPERTY(EditAnywhere, Category = "Anim", meta = (ClampMin = "0"))
	int32 SequenceIndex = 0;

	/**
	 * 재생 속도. **전환하는 순간에만 적용된다** — 렌더 쪽이 시퀀스 인덱스가 바뀔 때만
	 * 트랙을 다시 설정하기 때문이다. 속력에 맞춰 매 프레임 조절하는 용도로는 못 쓴다.
	 */
	UPROPERTY(EditAnywhere, Category = "Anim", meta = (ClampMin = "0.01"))
	float PlayRate = 1.f;

	/**
	 * 클립 길이(초, PlayRate 1 기준).
	 *
	 * 단발 동작(공격/사출/사격)에서는 **이 시간이 지나면 로코모션으로 되돌린다.**
	 * GPU가 재생 종료를 알려주지 않으므로 시간을 직접 재는 수밖에 없고, 되돌려 놓아야
	 * 다음 공격에서 인덱스가 다시 바뀌어 애니메이션이 재시작된다(같은 인덱스를 다시
	 * 써넣는 것만으로는 재생되지 않는다).
	 *
	 * 루프 동작(대기/이동)에서는 개체별 시작 위상을 흩는 데 쓴다. 0이면 흩지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Anim", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float Duration = 0.f;
};

/**
 * 행동 상태 → 인스턴스 스킨드 메시 시퀀스 인덱스 매핑.
 *
 * 원거리 표현(ISM)은 AnimBP를 거치지 않고 SequenceList의 인덱스 하나로 포즈가 정해진다.
 * 그 인덱스를 코드에 박으면 종류마다 애님 세트가 다른 순간 무너지므로, Config 애셋에서
 * 지정하게 둔다. 근거리 표현(액터 + AnimBP)은 이 값을 쓰지 않고 ActionState를 직접 읽는다.
 */
USTRUCT(BlueprintType)
struct FTREnemyAnimParams : public FMassConstSharedFragment
{
	GENERATED_BODY()

	// 기본값은 "대기, 이동, 공격, 사출, 사격, 죽음" 순으로 채운 SequenceList를 가정한다.
	FTREnemyAnimParams()
		: Idle(0)
		, Move(1)
		, Attack(2, 0.6f)
		, Launch(3, 0.5f)
		, Fire(4, 0.5f)
		, Death(5)
	{}

	/** 멈춰 있을 때. 루프. */
	UPROPERTY(EditAnywhere, Category = "Anim|Locomotion")
	FTRAnimSequenceRef Idle;

	/** 추적 중일 때. 루프. */
	UPROPERTY(EditAnywhere, Category = "Anim|Locomotion")
	FTRAnimSequenceRef Move;

	/** 근접형 자폭 준비. 단발. */
	UPROPERTY(EditAnywhere, Category = "Anim|Action")
	FTRAnimSequenceRef Attack;

	/** 사출형 상체 발사. 단발. */
	UPROPERTY(EditAnywhere, Category = "Anim|Action")
	FTRAnimSequenceRef Launch;

	/** 사격형 사격. 단발. */
	UPROPERTY(EditAnywhere, Category = "Anim|Action")
	FTRAnimSequenceRef Fire;

	/** 죽음. Duration을 0으로 두면 마지막 프레임에서 멈춘 채 유지된다. */
	UPROPERTY(EditAnywhere, Category = "Anim|Action")
	FTRAnimSequenceRef Death;

	/** 이 속력(cm/s)을 넘으면 이동, 아니면 대기. */
	UPROPERTY(EditAnywhere, Category = "Anim|Locomotion", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MoveSpeedThreshold = 50.f;

	/**
	 * 이동 → 대기로 돌아갈 때 임계값을 얼마나 낮출지(비율). 0.2면 들어갈 땐 50, 나올 땐 40.
	 *
	 * **이게 0이면 임계값 근처에서 애니메이션이 얼어붙는다.** 인덱스가 매 프레임 뒤집히면
	 * 렌더 쪽이 그때마다 트랙을 다시 설정해서 재생 위치가 계속 0으로 돌아가기 때문이다.
	 * 분리력으로 서로 밀어내는 무리가 정지 거리 근처에서 서성일 때 반드시 겪는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Anim|Locomotion", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float MoveSpeedHysteresis = 0.2f;

	/** 시퀀스가 바뀔 때의 크로스페이드 시간. 0이면 즉시 전환. */
	UPROPERTY(EditAnywhere, Category = "Anim", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float BlendTime = 0.15f;

	/**
	 * 루프 클립의 시작 위상을 개체마다 흩는다.
	 *
	 * 한 웨이브가 동시에 스폰되면 전원이 같은 프레임에 이동 상태로 들어가고, 재생 위치도
	 * 함께 0에서 시작해서 발이 완벽히 맞는 군무가 된다. 이걸 푸는 값이다.
	 * 해당 클립의 Duration이 0이면 위상을 계산할 길이가 없으므로 무시된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Anim|Locomotion")
	bool bDesyncLoopPhase = true;
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
		default:                     Approach.StopDistance = Params.ContactStopDistance; break;
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

	/**
	 * 이 개체의 속도 배수를 뽑는다. 스폰 시 한 번만 부른다.
	 *
	 * 균등 분포다. 정규 분포를 쓰면 대부분이 평균 근처에 몰려서 눈에 띄는 개체가
	 * 줄어드는데, 여기서 원하는 건 "빠른 놈과 느린 놈이 섞여 보이는 것"이다.
	 */
	inline float MakeSpeedScale(const FTRChaseParams& ChaseParams, FRandomStream& RandomStream)
	{
		const float Variance = FMath::Clamp(ChaseParams.SpeedVariance, 0.f, 0.9f);

		return 1.f - RandomStream.FRand() * Variance;
	}

	/**
	 * 이 개체가 목표로 접근할 때 내려는 속도(cm/s).
	 *
	 * 계산을 한 곳에 모아 둔다. 예전에는 조종 프로세서 안에 인라인으로 있었는데,
	 * 속도를 읽어야 하는 곳이 늘어날 때마다 같은 규칙을 다시 쓰게 되고
	 * 그중 하나가 개체별 배수를 빠뜨리면 그 경로에서만 무리가 다시 균일해진다.
	 *
	 * @param MovementMaxSpeed  FMassMovementParameters::MaxSpeed. 엔진의 최종 클램프 값.
	 */
	inline float ComputeDesiredSpeed(const FTRApproachFragment& Approach,
		float MovementMaxSpeed, const FTRChaseParams& ChaseParams)
	{
		// 명시된 값에는 SpeedRatio를 곱하지 않는다 — 이미 "얼마로 달릴지"가 정해진
		// 수치라 또 곱하면 기획 수치(상체 25 m/s)와 어긋난다.
		const float BaseSpeed = (Approach.MaxSpeed > 0.f)
			? Approach.MaxSpeed
			: MovementMaxSpeed * ChaseParams.SpeedRatio;

		// 개체별 배수는 어느 쪽 경로든 마지막에 곱한다. 사출된 상체도 원래 몸이
		// 느렸으면 느리게 날아가는 편이 일관되다.
		return BaseSpeed * Approach.SpeedScale;
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

	/** 이 행동 상태에 대응하는 단발 클립. 로코모션 상태면 nullptr. */
	inline const FTRAnimSequenceRef* FindOneShot(const FTREnemyAnimParams& Params, ETREnemyActionState ActionState)
	{
		switch (ActionState)
		{
		case ETREnemyActionState::Attack: return &Params.Attack;
		case ETREnemyActionState::Launch: return &Params.Launch;
		case ETREnemyActionState::Fire:   return &Params.Fire;
		case ETREnemyActionState::Death:  return &Params.Death;
		default:                          return nullptr;
		}
	}

	/**
	 * 속력으로 대기/이동을 고른다.
	 *
	 * @param bWasMoving  직전 프레임에 이동 클립이었는지. 이미 이동 중이면 더 낮은 속력까지
	 *                    버틴다. 같은 값을 양방향에 쓰면 임계값 위아래로 진동하는 개체가 매
	 *                    프레임 클립을 갈아치우고, 그게 곧 재생 위치 리셋이 된다.
	 */
	inline const FTRAnimSequenceRef& SelectLocomotion(const FTREnemyAnimParams& Params, float Speed, bool bWasMoving)
	{
		const float LeaveThreshold =
			Params.MoveSpeedThreshold * (1.f - FMath::Clamp(Params.MoveSpeedHysteresis, 0.f, 0.9f));
		const float Threshold = bWasMoving ? LeaveThreshold : Params.MoveSpeedThreshold;

		return (Speed > Threshold) ? Params.Move : Params.Idle;
	}

	/**
	 * 루프 클립의 재생 위치(초). 원점은 **월드 시각 0**이다.
	 *
	 * 로코모션은 진입 시각을 기록하지 않으므로 양쪽 표현이 공유할 원점이 월드 시각뿐이다.
	 * 시각의 함수라 언제 물어봐도 같은 값이고, 그래서 매 프레임 다시 써넣어도 재생이 리셋되지 않는다.
	 *
	 * @param PhaseOffset  0~1. 개체마다 다르게 주면 무리가 같은 발을 딛지 않는다.
	 */
	inline float ComputeLoopPosition(const FTRAnimSequenceRef& Sequence, float PhaseOffset, float WorldTime)
	{
		const float Length = Sequence.Duration;
		if (Length <= 0.f)
		{
			// 접을 길이를 모른다. 애셋에서 Duration을 채워야 위상이 산다.
			return 0.f;
		}

		const float PlayRate = FMath::Max(Sequence.PlayRate, KINDA_SMALL_NUMBER);

		// 월드 시각이 수천 초가 되면 float 유효 자릿수가 클립 길이를 넘어 위상이 계단처럼 튄다.
		const double Position = static_cast<double>(PhaseOffset) * Length
			+ static_cast<double>(WorldTime) * PlayRate;

		return static_cast<float>(FMath::Fmod(Position, static_cast<double>(Length)));
	}

	/** 단발 클립의 재생 위치(초). 원점은 상태 진입 시각 — 복제되는 값이라 클라에서도 맞는다. */
	inline float ComputeOneShotPosition(const FTRAnimSequenceRef& Sequence, float StateElapsed)
	{
		return FMath::Max(StateElapsed, 0.f) * FMath::Max(Sequence.PlayRate, KINDA_SMALL_NUMBER);
	}

	/** ResolveAnim의 결과. 표현이 이 프레임에 재생할 것 전부. */
	struct FAnimResolution
	{
		/** 지금 고른 클립. 한 칸만 재생할 수 있는 ISM이 쓴다. */
		FTRAnimSequenceRef Sequence;

		bool bLooping = true;

		/** 루프가 이동 클립인가. 다음 프레임 히스테리시스와 AnimBP 전이 조건으로 쓴다. */
		bool bMoving = false;

		/**
		 * 지금 행동 상태에 대응하는 단발 클립이 있는가. 없으면 ActionPosition은 의미가 없다.
		 *
		 * 받는 쪽은 이 값이 false면 **직전 값을 유지해야 한다.** 0으로 덮으면 단발이 끝나
		 * 블렌드 아웃되는 중에 상태가 None으로 돌아갈 때 클립이 첫 프레임으로 되감긴다.
		 */
		bool bHasAction = false;

		/**
		 * 로코모션·단발 두 클립의 재생 위치(초). **고르지 않은 쪽도 계속 흐른다.**
		 *
		 * 액터 표현은 전이 중에 두 클립을 동시에 물고 블렌드한다. 고른 쪽만 갱신하면
		 * 물러나는 클립이 얼어붙은 채로 사라져서, 전환마다 눈에 띄는 끊김이 생긴다.
		 * ISM은 어차피 한 칸이라 GetPosition()만 본다.
		 */
		float LocomotionPosition = 0.f;
		float ActionPosition = 0.f;

		/** 고른 클립의 재생 위치. */
		float GetPosition() const { return bLooping ? LocomotionPosition : ActionPosition; }
	};

	/** 표현이 이 프레임에 보는 개체 상태 한 벌. ResolveAnim의 입력이자 액터에 넘기는 값. */
	struct FAnimContext
	{
		ETREnemyActionState ActionState = ETREnemyActionState::None;

		/** 상태에 들어간 뒤 경과 시간(초). 단발 재생 위치의 원점이다. */
		float StateElapsed = 0.f;

		/** 월드 속도(cm/s). */
		FVector Velocity = FVector::ZeroVector;

		/**
		 * 지면이 흐르는 월드 속도(cm/s). 속력은 Velocity와의 차로 잰다 —
		 * 월드 속력을 쓰면 기차와 나란히 달리는 적이 대기 모션을 재생하며 미끄러진다.
		 */
		FVector GroundVelocity = FVector::ZeroVector;

		/** 직전 프레임의 이동 판정. 임계값 히스테리시스에 쓴다. */
		bool bWasMoving = false;

		/** GetTypeHash(FMassEntityHandle). 루프 위상을 개체마다 흩는 씨앗. */
		uint32 EntityHash = 0;

		float WorldTime = 0.f;
	};

	/**
	 * 이 프레임에 무엇을 어디서 재생할지 결정한다. **두 표현 모두 이 함수만 쓴다.**
	 *
	 * ISM은 렌더가 기준 시각에서 위치를 역산하고 액터는 AnimBP가 누적하는데, 둘이 어긋난
	 * 채로 LOD가 바뀌면 포즈가 튄다. 판단과 위상을 여기 한 곳에 두면 어긋날 수가 없다.
	 *
	 * 상태 프래그먼트를 건드리지 않는 것도 요점이다. 단발이 끝났다고 ActionState를 되돌리면
	 * 공격 프로세서의 "이미 Attack이면 준비 시간을 다시 세지 않는다" 판정이 깨진다.
	 * 여기서는 표현만 로코모션으로 되돌리고, 상태는 전투 쪽이 계속 쥐고 있게 둔다.
	 */
	inline FAnimResolution ResolveAnim(const FTREnemyAnimParams& Params, const FAnimContext& Ctx)
	{
		const float Speed = (Ctx.Velocity - Ctx.GroundVelocity).Size2D();

		FAnimResolution Out;

		// ── 로코모션 ─────────────────────────────────────────────────────────
		// 단발 중에도 계산한다. 액터가 전이 구간에 두 클립을 함께 물기 때문이고,
		// 원점이 월드 시각이라 지금 재생 중인지와 무관하게 값이 정해진다.
		const FTRAnimSequenceRef& LoopSequence = SelectLocomotion(Params, Speed, Ctx.bWasMoving);
		Out.bMoving = (LoopSequence.SequenceIndex == Params.Move.SequenceIndex);

		// 해시로 위상을 뽑는다. 난수는 매 프레임 값이 바뀌어 재생 위치가 튀고,
		// 스폰 때 뽑아 두면 프래그먼트가 늘고 복제도 해야 한다.
		const float PhaseOffset = Params.bDesyncLoopPhase
			? static_cast<float>(Ctx.EntityHash & 0xFFFF) / 65535.f
			: 0.f;

		Out.LocomotionPosition = ComputeLoopPosition(LoopSequence, PhaseOffset, Ctx.WorldTime);

		// ── 단발 ─────────────────────────────────────────────────────────────
		const FTRAnimSequenceRef* OneShot = FindOneShot(Params, Ctx.ActionState);
		bool bOneShotPlaying = false;

		if (OneShot != nullptr)
		{
			Out.bHasAction = true;

			// Duration이 0이면 "끝나지 않는 단발"이다. 죽음 포즈를 유지하는 용도.
			const bool bUnbounded = (OneShot->Duration <= 0.f);
			const float RawPosition = ComputeOneShotPosition(*OneShot, Ctx.StateElapsed);

			bOneShotPlaying = bUnbounded || (RawPosition < OneShot->Duration);

			// 끝난 뒤에도 위치를 넘긴다 — 블렌드 아웃 동안 클립이 얼면 안 되기 때문이다.
			// 대신 클립 끝에 붙인다. ISM 쪽도 LoopMode::Clamp라 같은 자리에서 멈춘다.
			Out.ActionPosition = bUnbounded ? RawPosition : FMath::Min(RawPosition, OneShot->Duration);
		}

		// ── 이 프레임에 화면을 쥐는 쪽 ────────────────────────────────────────
		Out.bLooping = !bOneShotPlaying;
		Out.Sequence = bOneShotPlaying ? *OneShot : LoopSequence;

		return Out;
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
		Approach.StopDistance = Params.ContactStopDistance;
		Approach.LeaveDistance = Params.ContactStopDistance + LeaveDistanceMargin;
		Approach.MaxSpeed = Params.LaunchSpeed;
		Approach.bLockTargetZone = false;

		Combat.MaxHealth = Params.LaunchedHealth;
		Combat.Health = FMath::Min(Combat.Health, Params.LaunchedHealth);

		// 사출된 상체를 요격해도 자원은 나오지 않는다(기획서).
		Combat.ResourceValue = 0;
	}
}

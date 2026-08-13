// TRMassFragments.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/Core/TRMassTypes.h"
#include "TRMassFragments.generated.h"

/** 추적 엔티티 식별용. TR Chaser 트레이트가 붙인다. */
USTRUCT()
struct FTRChaserTag : public FMassTag
{
	GENERATED_BODY()
};

/** 목표 정지 거리 안에 들어온 엔티티. Steering이 부여/해제하고, 공격 프로세서가 필터로 쓴다. */
USTRUCT()
struct FTRArrivedTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * 폭발 예약됨. 준비 시간이 흐르는 중이며, 끝나면 피해를 넣고 사라진다.
 *
 * 태그로 두는 이유는 "이미 예약했는가"를 아키타입 단위로 걸러내기 위해서다.
 * 불리언 필드로 두면 매 틱 전체 엔티티를 훑으며 검사해야 한다.
 */
USTRUCT()
struct FTRDetonatingTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * 사출형의 상체가 날아가는 중.
 *
 * 하체를 따로 엔티티로 만들지 않는다. 같은 엔티티가 이 태그를 얻는 순간
 * 체력·속도·정지 거리가 상체 값으로 바뀌고, 표현(메시)만 교체하면 된다.
 * 엔티티를 죽이고 새로 만들면 스폰 비용과 표현 전환 깜빡임이 함께 생긴다.
 */
USTRUCT()
struct FTRLaunchedTag : public FMassTag
{
	GENERATED_BODY()
};

//----------------------------------------------------------------------//
//  추적
//----------------------------------------------------------------------//

/** 이 엔티티가 어느 채널을 추적하는지. 런타임에 바꾸면 추적 대상이 바뀐다. */
USTRUCT()
struct FTRChaseChannelFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	ETRTargetChannel Channel = ETRTargetChannel::Train;
};

/** TargetSync 프로세서가 매 틱 채워주는 캐시. Steering 프로세서는 이것만 읽는다. */
USTRUCT()
struct FTRTargetFragment : public FMassFragment
{
	GENERATED_BODY()

	// 목표 위치
	UPROPERTY()
	FVector TargetLocation = FVector::ZeroVector;

	// 목표 유효성 체크 
	UPROPERTY()
	bool bHasTarget = false;

	// 도착여부
	UPROPERTY()
	bool bArrived = false;
};

/**
 * 이 개체가 어디까지 접근하고 얼마나 빠른가.
 *
 * 종류·상태에 따라 달라지는 값만 모았다 — 사출형은 40m에서 멈췄다가 상체가 되면
 * 접촉까지 붙고, 사격형은 55m를 유지한다. 공유 파라미터(FTREnemyCombatParams)는
 * 값이 같은 엔티티끼리 합쳐지는 구조라 런타임에 개체별로 바꿀 수 없어서 여기로 뺐다.
 *
 * 값은 UE::TR::Enemy::MakeApproach 계열이 종류에서 계산해 준다. 직접 채우지 말 것.
 */
USTRUCT()
struct FTRApproachFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 목표 표면에서 이 거리까지만 접근하고 멈춘다. */
	UPROPERTY()
	float StopDistance = 300.f;

	/** 이 거리 밖으로 밀려나면 도달이 풀린다. StopDistance보다 커야 태그가 떨리지 않는다. */
	UPROPERTY()
	float LeaveDistance = 350.f;

	/** 최대 속도(cm/s). 0이면 FMassMovementParameters::MaxSpeed를 쓴다(=대부분의 적). */
	UPROPERTY()
	float MaxSpeed = 0.f;
};

//----------------------------------------------------------------------//
//  상태
//----------------------------------------------------------------------//

/** 행동 상태와 진입 시각. AnimBP가 몽타주 대신 이 값으로 재생 위치를 잡는다. */
USTRUCT()
struct FTREnemyStateFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	ETREnemyActionState ActionState = ETREnemyActionState::None;

	/** 상태에 들어간 월드 시각. 경과 시간 계산용. */
	UPROPERTY()
	float StateEnteredTime = 0.f;
};

/**
 * 행동 상태 진입. 상태와 진입 시각은 항상 함께 바뀌어야 한다.
 *
 * 따로 대입하면 한쪽만 갱신하는 실수가 나고, 그러면 AnimBP가 새 상태를
 * 옛날 경과 시간으로 재생해서 애니메이션이 중간부터 시작한다.
 */
inline void EnterActionState(FTREnemyStateFragment& State, ETREnemyActionState NewState, float WorldTime)
{
	State.ActionState = NewState;
	State.StateEnteredTime = WorldTime;
}

//지면 상태 체크
USTRUCT()
struct FTRGroundFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 마지막으로 찾은 지면 높이. */
	UPROPERTY()
	float GroundZ = 0.f;

	/** 다음 트레이스 시각. 개체마다 흩어놓아 한 프레임에 몰리지 않게 한다. */
	UPROPERTY()
	float NextTraceTime = 0.f;

	UPROPERTY()
	bool bHasGround = false;
};

/**
 * 개체별 전투 수치. 공유 파라미터에 티어 배수를 곱한 결과가 스폰 시 여기 들어온다.
 *
 * 파라미터를 그대로 읽지 않고 복사해 두는 이유는 티어 스케일링 때문이다.
 * 웨이브마다 값이 달라지므로 공유할 수 없고, 매번 배수를 곱해 읽으면
 * "현재 체력"을 최대치와 비교할 기준이 흔들린다.
 *
 * 값은 UE::TR::Enemy::InitCombat이 채운다. 직접 채우지 말 것.
 */
USTRUCT()
struct FTREnemyCombatFragment : public FMassFragment
{
	GENERATED_BODY()

	/**
	 * 남은 체력.
	 *
	 * 아직 이 값을 깎는 경로가 없다(포탑 → Mass 엔티티 피해가 미구현).
	 * 스폰 시 채워지기만 하고 읽히지 않는 상태다.
	 */
	UPROPERTY()
	float Health = 25.f;

	UPROPERTY()
	float MaxHealth = 25.f;

	/** 접촉 폭발 피해. 기획서의 자폭 피해량·충돌 피해량. */
	UPROPERTY()
	float ContactDamage = 15.f;

	/** 사격 1발 피해. 사격형만 쓴다. */
	UPROPERTY()
	float ShotDamage = 25.f;

	/** 처치 시 주는 자원. 자폭·자체 폭발로 사라지면 주지 않는다(기획서). */
	UPROPERTY()
	int32 ResourceValue = 1;

	/** 다음 행동(사격, 폭발) 예정 시각(월드 초). */
	UPROPERTY()
	float NextActionTime = 0.f;

	/** 지금까지 쏜 발수. 사격형이 탄을 소진했는지 판단한다. */
	UPROPERTY()
	int32 ShotsFired = 0;
};

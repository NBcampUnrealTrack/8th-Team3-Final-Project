// TRMassFragments.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
// SetArrived 가 엔티티 핸들을 받는다. MassEntityTypes.h 는 이걸 끌어오지 않는다.
#include "Mass/EntityHandle.h"
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

	/** 고정해 둔 표적 구역 번호. INDEX_NONE 이면 매 틱 가장 가까운 구역을 쓴다. */
	UPROPERTY()
	int32 LockedZone = INDEX_NONE;
};

/**
 * 도달 상태를 바꾼다. **불리언과 태그는 항상 함께 움직여야 한다.**
 *
 * 같은 사실이 두 군데 있는 건 의도된 중복이다. 태그 변경은 Defer 라 같은 프레임의
 * 쿼리 필터에 반영되지 않아서, 그 프레임 안에서 판단하려면 불리언이 필요하다.
 * 문제는 둘을 맞추는 규칙이 호출부마다 흩어져 있었다는 점이다 — 한쪽만 갱신하면
 * 도달 판정이 태그와 불리언 사이에서 어긋나고, 그건 재현이 어려운 종류의 버그다.
 *
 * 커맨드 버퍼 타입을 템플릿으로 받는 이유는 이 헤더가 프로젝트 전역에 포함되기
 * 때문이다. MassCommandBuffer.h 를 여기서 끌어오면 전체 컴파일이 무거워진다.
 */
template<typename TMassCommandBuffer>
inline void SetArrived(TMassCommandBuffer& Commands, FMassEntityHandle Entity,
	FTRTargetFragment& Target, bool bArrived)
{
	Target.bArrived = bArrived;

	if (bArrived)
	{
		Commands.template AddTag<FTRArrivedTag>(Entity);
	}
	else
	{
		Commands.template RemoveTag<FTRArrivedTag>(Entity);
	}
}

/**
 * 복제용 식별자. 서버가 스폰 시 부여하고, 클라이언트는 이 번호로 같은 적을 찾는다.
 *
 * FMassEntityHandle 을 그대로 쓸 수 없다. 핸들은 그 월드의 로컬 인덱스라
 * 서버의 3번과 클라이언트의 3번이 다른 개체다.
 */
USTRUCT()
struct FTRMassNetIdFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 0이면 아직 부여되지 않았다는 뜻. 서버에서만 채운다. */
	UPROPERTY()
	int32 NetId = 0;

	/**
	 * 이 적을 어떤 Config 로 표현할지. 클라이언트가 같은 메시로 스폰하기 위해 필요하다.
	 * 스냅샷에 애셋 경로를 실어 보내는 대신 팔레트 색인만 보낸다(1바이트).
	 */
	UPROPERTY()
	uint8 ConfigIndex = 0;
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

	/**
	 * 이 개체만의 속도 배수. 스폰 시 한 번 뽑고 평생 유지한다.
	 *
	 * 속도가 전부 공유 파라미터에서 나오면 한 무리가 통째로 같은 속도로 움직여서,
	 * 대열이 얼어붙은 것처럼 보인다. 분리력이 좌우로는 흩어주지만 앞뒤로는
	 * 아무것도 하지 않기 때문에 특히 눈에 띈다.
	 *
	 * 매 틱 흔들지 않고 개체에 고정하는 이유는, 프레임마다 다시 뽑으면 평균으로
	 * 수렴해서 결국 다 같은 속도가 되고 애니메이션 재생 속도만 떨리기 때문이다.
	 * "저 녀석이 유독 빠르다"가 보여야 무리처럼 읽힌다.
	 *
	 * 값은 UE::TR::Enemy::MakeSpeedScale이 뽑는다. 직접 채우지 말 것.
	 */
	UPROPERTY()
	float SpeedScale = 1.f;

	/** 표적 구역을 한 번 정하면 고정한다. 사격형이 쓴다. */
	UPROPERTY()
	bool bLockTargetZone = false;
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
 * 행동 상태 진입. **이미 그 상태면 아무것도 하지 않고 false를 돌려준다.**
 *
 * 상태와 진입 시각은 항상 함께 바뀌어야 한다. 따로 대입하면 한쪽만 갱신하는 실수가
 * 나고, 그러면 표현이 새 상태를 옛날 경과 시간으로 재생한다.
 *
 * 반환값이 핵심이다. 같은 행동을 StateTree 태스크와 프로세서가 각자 수행하는 구간이
 * 있는데(근접형 자폭, 사출), 예전에는 호출부마다 `if (State.ActionState == X) break;`를
 * 손으로 적어 중복을 피했다. 그 가드는 세 곳에서 모양이 제각각이었고 사격형에는
 * 아예 없었다. 이제는 진입에 성공한 쪽만 뒷일을 하면 된다:
 *
 *     if (!TryEnterActionState(State, ETREnemyActionState::Attack, WorldTime))
 *     {
 *         break;   // 다른 쪽이 이미 처리했다
 *     }
 *     ...준비 시간 설정, 태그 부착...
 *
 * 멱등이라는 점도 중요하다. 매 틱 불려도 StateEnteredTime이 갱신되지 않으므로,
 * 경과 시간으로 재생 종료를 판단하는 표현(UE::TR::Enemy::ResolveAnim)이 단발
 * 애니메이션의 마지막 프레임에서 영원히 멈추는 일이 없다.
 *
 * @return 상태가 실제로 바뀌었으면 true. 이미 같은 상태였으면 false.
 */
inline bool TryEnterActionState(FTREnemyStateFragment& State, ETREnemyActionState NewState, float WorldTime)
{
	if (State.ActionState == NewState)
	{
		return false;
	}

	State.ActionState = NewState;
	State.StateEnteredTime = WorldTime;
	return true;
}

/**
 * 같은 상태를 처음부터 다시 시작한다. 진입 시각을 무조건 갱신한다.
 *
 * 한 상태 안에서 **여러 번 반복되는 동작**에만 쓴다 — 사격형이 FireInterval마다
 * 한 발씩 쏘는 경우가 유일한 예다. 표현 쪽은 진입 시각이 바뀌는 것을 "다시 재생하라"는
 * 신호로 읽으므로, 이것 없이 TryEnterActionState만 쓰면 두 번째 발부터 사격
 * 애니메이션이 나오지 않는다.
 *
 * 반대로 1회성 행동에 이걸 쓰면 매 틱 재생이 리셋되어 애니메이션이 얼어붙는다.
 * 헷갈리면 TryEnterActionState 쪽이 맞다.
 */
inline void RetriggerActionState(FTREnemyStateFragment& State, ETREnemyActionState NewState, float WorldTime)
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

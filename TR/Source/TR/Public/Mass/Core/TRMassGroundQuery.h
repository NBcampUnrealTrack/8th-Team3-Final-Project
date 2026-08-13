// TRMassGroundQuery.h

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

/**
 * 지면 탐색 공용 함수.
 *
 * 스폰 배치와 이동 중 지면 추종이 같은 판정("걸을 수 있는 면인가")을 쓰므로 여기 모은다.
 * 나뉘어 있으면 경사 기준 같은 값이 한쪽만 바뀌어 두 시점의 높이가 달라진다.
 */
namespace UE::TR::MassGround
{
	/** 걸을 수 있다고 볼 최소 경사(법선 Z). 약 45도. */
	inline constexpr float MinWalkableNormalZ = 0.7f;

	/**
	 * 아래로 훑어 **가장 낮은** 걸을 수 있는 면을 찾는다.
	 *
	 * 첫 충돌을 쓰면 기차 지붕이나 구조물 위가 잡혀서 적이 공중에서 나타난 것처럼 보인다.
	 * 스폰처럼 "확실히 바닥에 놓아야" 하는 경우에 쓴다.
	 * (지면 위에 다리가 있는 맵이라면 이 규칙을 다시 봐야 한다.)
	 */
	TR_API bool FindLowestGroundZ(const UWorld& World, const FVector& Location,
		float TraceUpOffset, float TraceDownOffset, const AActor* IgnoredActor, float& OutGroundZ);

	/**
	 * 바로 아래의 걸을 수 있는 면을 찾는다.
	 *
	 * 이미 지면 근처에 있는 개체의 높이를 갱신하는 용도라 첫 충돌이면 충분하다.
	 * 매 프레임은 아니어도 자주 불리므로 단일 트레이스를 쓴다.
	 */
	TR_API bool FindGroundZBelow(const UWorld& World, const FVector& Location,
		float TraceUpOffset, float TraceDownOffset, float& OutGroundZ);
}

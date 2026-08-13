// TRMassTypes.h

#pragma once

#include "CoreMinimal.h"
#include "TRMassTypes.generated.h"

//타겟 채널
UENUM(BlueprintType)
enum class ETRTargetChannel : uint8
{
	Train,      
	Player,     
	Custom1,
	Custom2,

	MAX UMETA(Hidden)
};

//적상태
UENUM(BlueprintType)
enum class ETREnemyType : uint8
{
	Melee,
	Launcher,
	Shooter,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ETRSpawnShape : uint8
{
	Annulus,
	Box,
	Line,
};

//애니메이션 상태
UENUM(BlueprintType)
enum class ETREnemyActionState : uint8
{
	None,       // 이동 중 등 특별한 행동 없음
	Attack,     // 근접형 자폭 준비
	Launch,     // 사출형 상체 발사
	Fire,       // 사격형 사격
	Death,

	MAX UMETA(Hidden)
};

namespace UE::TR::Mass
{
	/**
	 * 이 태그가 붙은 프리미티브 컴포넌트가 있으면, 메시 전체 바운드 대신
	 * 그 컴포넌트를 적의 추적·접촉 판정 영역으로 쓴다(기차의 TargetVolume).
	 *
	 * 태그로 찾는 이유는 Mass 쪽이 기차 클래스를 몰라도 되게 하기 위해서다.
	 * 나중에 다른 액터를 목표로 삼아도 박스 하나에 이 태그만 붙이면 된다.
	 *
	 * 여기(가장 가벼운 공용 헤더)에 두어, 태그를 붙이는 쪽이 Mass 서브시스템
	 * 헤더를 통째로 끌어오지 않게 한다.
	 *
	 * 정적 초기화 순서 문제를 피하려고 함수 안의 지역 static으로 둔다.
	 */
	inline FName GetTargetVolumeTagName()
	{
		static const FName Tag(TEXT("TRMassTargetVolume"));
		return Tag;
	}
}

//Debug용
inline const TCHAR* LexToString(ETREnemyType Type)
{
	switch (Type)
	{
	case ETREnemyType::Launcher: return TEXT("사출형");
	case ETREnemyType::Shooter:  return TEXT("사격형");
	default:                     return TEXT("근접형");
	}
}

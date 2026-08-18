// TRGroundMotionSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Subsystems/WorldSubsystem.h"
#include "TRGroundMotionSubsystem.generated.h"

/**
 * 지면이 흐르는 속도. ATRLevelSceneManager가 매기고 Mass 프로세서들이 읽는다.
 *
 * 기차가 고정되고 지형이 스크롤하므로 **월드 속도가 0인 적도 실제로는 달리고 있다.**
 * 애니메이션이 월드 속도를 보면 그런 적이 대기 모션을 재생하며 지면 위를 미끄러진다.
 *
 * 쓰는 쪽은 매니저 하나뿐이고 Mass 처리 중에는 쓰지 않으므로 락이 없다.
 */
UCLASS()
class TR_API UTRGroundMotionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 지면이 흐르는 월드 속도(cm/s). 스크롤을 구현한 쪽이 매 틱 매긴다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Ground")
	void SetGroundVelocity(const FVector& InGroundVelocity) { GroundVelocity = InGroundVelocity; }

	UFUNCTION(BlueprintPure, Category = "TR|Ground")
	FVector GetGroundVelocity() const { return GroundVelocity; }

private:
	/** 0은 "지면이 멈춰 있다". 스크롤이 없는 레벨에서는 지면 대비 속력이 곧 월드 속력이 된다. */
	FVector GroundVelocity = FVector::ZeroVector;
};

template<>
struct TMassExternalSubsystemTraits<UTRGroundMotionSubsystem> final
{
	enum
	{
		// 값 하나를 읽기만 한다.
		GameThreadOnly = false,
		ThreadSafeWrite = false
	};
};

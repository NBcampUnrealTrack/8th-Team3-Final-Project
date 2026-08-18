// TREnemyAnimInstance.cpp

#include "Mass/Representation/TREnemyAnimInstance.h"

#include "Mass/Representation/TRMassEnemyActor.h"

void UTREnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	//여기서 한 번만 캐스팅한다. 그래프에서 매 프레임 하면 Fast Path가 깨진다
	OwnerEnemy = Cast<ATRMassEnemyActor>(GetOwningActor());
}

void UTREnemyAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 리셋 플래그만 게임 스레드에서 소비한다. 액터에 쓰는 동작이라 병렬 평가 중에 하면 안 된다.
	bResetPose = (OwnerEnemy != nullptr) && OwnerEnemy->ConsumePoseReset();
}

void UTREnemyAnimInstance::NativeThreadSafeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (OwnerEnemy == nullptr)
	{
		return;
	}

	//게터를 여러 번 부르지 않는다. 구조체 하나를 받아 옮기기만 한다
	const FTREnemyAnimData& Data = OwnerEnemy->GetAnimData();

	GroundSpeed = Data.GroundSpeed;
	LocalVelocity = Data.LocalVelocity;
	ActionState = Data.ActionState;
	bPlayingOneShot = Data.bPlayingOneShot;
	bIsMoving = Data.bIsMoving;
	ActionTime = Data.ActionTime;
	LocomotionTime = Data.LocomotionTime;

	// 클립이 내는 속력과 실제 지면 대비 속력의 비.
	// 이 게임은 보정폭이 크다 — 기차와 나란히 달리는 적조차 지면 대비로는 전속력이다.
	const float RootSpeed = FMath::Max(RunClipRootSpeed, 1.f);
	const float MaxRate = FMath::Max(MaxLocomotionPlayRate, MinLocomotionPlayRate);

	LocomotionPlayRate = FMath::Clamp(GroundSpeed / RootSpeed, MinLocomotionPlayRate, MaxRate);
}

// BANG! 평상시 시점 — 마우스 "위치" 로 카메라를 돌린다. 드래그가 필요 없다.
//
// 예전에는 클릭한 채로 마우스를 움직여야 돌아갔다(Enhanced Input IA_Look, 블루프린트 바인딩 +
// ABGCharacter::PrimaryCamera 의 bUsePawnControlRotation). 그 경로는 끊었다
// (PrimaryCamera->bUsePawnControlRotation = false — ABGCharacter 생성자 참고). 대신 이
// 컴포넌트가 화면에서 마우스의 가로 위치만 보고 카메라 Yaw 를 직접 정한다.
// 화면 가운데 = 정면, 가장자리 = MaxYawDeg 만큼 돌아간다. 영원히 도는 것은 없다 —
// 마우스가 가운데로 돌아오면 카메라도 같이 돌아온다.
//
// BGWorldTargetingComponent 의 조준 가장자리 팬과 "카메라 Yaw 오프셋을 쓴다" 는 점이 같다.
// 충돌을 피하려고 조준 중에는 이 컴포넌트가 쉰다 — 그쪽이 같은 카메라를 쓰는 동안은 양보한다.
// 기준 자세(원래 각도)는 둘 다 ABGCharacter::SetPrimaryCameraYawOffset 안에서 공유한다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BGCameraLookComponent.generated.h"

class ABGPlayerController;

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGCameraLookComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGCameraLookComponent();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 화면 가운데에서 가장자리까지 갔을 때 돌아가는 최대 각도(도). 가운데=0, 가장자리=±이 값. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Camera|Look", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxYawDeg = 30.f;

	/** 목표 각도를 따라가는 속도(초당 도). BGWorldTargetingComponent::RecenterSpeedDeg 와 같은 성격. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Camera|Look", meta = (ClampMin = "1.0"))
	float FollowSpeedDeg = 180.f;

private:
	/** 마우스 화면 위치 → 목표 Yaw(도). 뷰포트/커서를 못 읽으면 0(정면). */
	static float ComputeTargetYaw(const ABGPlayerController& PC, float MaxYaw);

	//	따라가는 중인 현재 오프셋. 목표가 0(정면 · 조준 중)이면 여기로 되돌아간다.
	float CurrentYawDeg = 0.f;
};

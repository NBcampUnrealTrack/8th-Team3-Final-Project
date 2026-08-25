// TREnvironmentBlenderComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TREnvironmentBlenderComponent.generated.h"

class AExponentialHeightFog;
class ADirectionalLight;
class ASkyLight;
class UPostProcessComponent;
class UTRBiomeEnvironment;

/**
 * 환경 프리셋 두 벌을 섞어 레벨의 실제 액터에 적용한다.
 *
 * **라이팅을 만지는 코드는 여기 하나뿐이어야 한다.** 안개는 레벨 블루프린트에서, 해는
 * 시퀀서에서, 색보정은 볼륨에서 각각 바뀌기 시작하면 두 달 뒤에 "왜 3웨이브만 노랗지"를
 * 아무도 못 찾는다. 끄는 지점을 하나로 두는 것이 이 컴포넌트의 존재 이유다.
 *
 * 언제 얼마나 섞을지는 정하지 않는다. ATRLevelSceneManager 가 매 틱 ApplyBlend 로
 * 알려 준다 — 그래야 하늘이 발밑 지면과 같은 근거로 움직인다.
 */
UCLASS(ClassGroup = (TR), meta = (BlueprintSpawnableComponent))
class TR_API UTREnvironmentBlenderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTREnvironmentBlenderComponent();

	/**
	 * From 에서 To 로 Alpha 만큼 간 상태를 레벨에 적용한다.
	 *
	 * @param From  출발 풍경. nullptr 이면 To 를 즉시 적용한다(판 시작 등).
	 * @param To    목표 풍경. nullptr 이면 **아무것도 하지 않는다** — 환경을 정의하지
	 *              않은 바이옴은 "레벨 기본값을 그대로 둔다"는 뜻으로 본다.
	 * @param Alpha 0~1.
	 */
	void ApplyBlend(const UTRBiomeEnvironment* From, const UTRBiomeEnvironment* To, float Alpha);

protected:
	virtual void BeginPlay() override;

private:
	/** 레벨에서 대상 액터를 찾아 둔다. 지정이 없으면 종류별로 첫 번째를 쓴다. */
	void ResolveTargets();

	/**
	 * 라이트가 Static 이면 경고한다.
	 *
	 * Static 라이트는 런타임 변경을 **조용히 버린다.** 에러도 경고도 없이 해만 그대로라,
	 * 값이 안 들어가는 줄 알고 프리셋을 의심하게 된다. (안개는 이 제약이 없다.)
	 */
	void WarnIfNotMovable(const USceneComponent* LightComponent, const TCHAR* Label) const;

	/** 안개를 쓸 것인가. 끄면 레벨의 안개를 건드리지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Environment|Blend")
	bool bBlendFog = true;

	UPROPERTY(EditAnywhere, Category = "Environment|Blend")
	bool bBlendSun = true;

	UPROPERTY(EditAnywhere, Category = "Environment|Blend")
	bool bBlendSkyLight = true;

	UPROPERTY(EditAnywhere, Category = "Environment|Blend")
	bool bBlendPostProcess = true;

	/**
	 * 대상 액터. 비워 두면 레벨에서 그 종류의 첫 번째를 찾아 쓴다.
	 *
	 * 안개나 해가 레벨에 여럿이면 직접 지정할 것. 자동 탐색은 순서를 보장하지 않아서,
	 * 어느 날 갑자기 다른 액터를 잡아도 이상하지 않다.
	 */
	UPROPERTY(EditInstanceOnly, Category = "Environment|Targets")
	TObjectPtr<AExponentialHeightFog> FogActor;

	UPROPERTY(EditInstanceOnly, Category = "Environment|Targets")
	TObjectPtr<ADirectionalLight> SunActor;

	UPROPERTY(EditInstanceOnly, Category = "Environment|Targets")
	TObjectPtr<ASkyLight> SkyLightActor;

	/**
	 * 포스트프로세스 우선순위. 레벨에 있는 볼륨보다 높아야 이쪽이 위에 덮인다.
	 *
	 * 두 컴포넌트가 이 값과 이 값 + 1 을 쓴다. 바탕(From)이 아래, 목표(To)가 위다.
	 */
	UPROPERTY(EditAnywhere, Category = "Environment|Blend")
	float PostProcessPriority = 100.f;

	/**
	 * 포스트프로세스를 섞는 두 겹.
	 *
	 * **FPostProcessSettings 를 손으로 섞지 않으려고 이렇게 한다.** 필드가 200 개가
	 * 넘어서 하나씩 Lerp 하면 반드시 빠뜨리고, 누가 새 필드를 켜면 그것만 안 섞인다.
	 * 바탕을 가중치 1 로 깔고 위에 목표를 Alpha 로 얹으면 엔진이 전부 섞어 준다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> PostProcessFrom;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> PostProcessTo;

	/**
	 * 마지막으로 얹은 프리셋. 같으면 다시 복사하지 않는다.
	 *
	 * FPostProcessSettings 는 1KB 가 넘는 구조체다. 매 틱 두 번 복사할 이유가 없다 —
	 * 프리셋이 바뀌는 순간에만 복사하고 그 뒤로는 가중치만 움직인다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<const UTRBiomeEnvironment> AppliedPostProcessFrom;

	UPROPERTY(Transient)
	TObjectPtr<const UTRBiomeEnvironment> AppliedPostProcessTo;
};

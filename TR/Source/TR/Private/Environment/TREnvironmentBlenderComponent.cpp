// TREnvironmentBlenderComponent.cpp

#include "Environment/TREnvironmentBlenderComponent.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Debug/TRGameplayLog.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Environment/TRBiomeEnvironment.h"
#include "EngineUtils.h"

UTREnvironmentBlenderComponent::UTREnvironmentBlenderComponent()
{
	// 스스로 틱하지 않는다. 매니저가 지면 전환 진행도를 계산한 **뒤에** 불러야
	// 하늘과 땅이 같은 프레임의 같은 값을 쓴다. 각자 틱하면 한 프레임씩 어긋난다.
	PrimaryComponentTick.bCanEverTick = false;
}

//대상 액터를 찾고 라이트 이동성을 확인한다
void UTREnvironmentBlenderComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveTargets();

	if (bBlendPostProcess)
	{
		AActor* Owner = GetOwner();

		// 언바운드 포스트프로세스 두 겹. 볼륨을 레벨에 놓게 하지 않는 이유는, 그러면
		// 프리셋은 애셋에 있는데 적용 대상은 레벨에 있어서 둘이 따로 놀기 때문이다.
		PostProcessFrom = NewObject<UPostProcessComponent>(Owner);
		PostProcessTo = NewObject<UPostProcessComponent>(Owner);

		float Priority = PostProcessPriority;
		for (UPostProcessComponent* Component : {PostProcessFrom.Get(), PostProcessTo.Get()})
		{
			Component->bUnbound = true;
			Component->Priority = Priority;
			Component->BlendWeight = 0.f;
			Component->SetupAttachment(Owner->GetRootComponent());
			Component->RegisterComponent();
			Priority += 1.f;
		}
	}
}

/**
 * 지정이 없으면 레벨에서 그 종류의 첫 번째를 찾고, 그래도 없으면 경고한다.
 *
 * 세 대상이 글자만 다르고 하는 일이 같아서 하나로 묶었다. 따로 두면 셋 중 하나만
 * 고치고 나머지를 잊기 좋다 — 실제로 "찾기"와 "경고"가 따로 놀아서 같은 조건을
 * 두 번씩 쓰고 있었다.
 */
template <typename TActorType>
static void ResolveEnvironmentTarget(UWorld& World, const bool bEnabled,
	TObjectPtr<TActorType>& Target, const TCHAR* ActorLabel, const TCHAR* SystemLabel)
{
	if (!bEnabled)
	{
		return;
	}

	if (Target == nullptr)
	{
		for (TActorIterator<TActorType> It(&World); It; ++It)
		{
			Target = *It;
			break;
		}
	}

	// 못 찾은 것은 조용히 넘기지 않는다. "환경이 안 바뀐다"는 증상만 보고 원인을
	// 찾으려면, 프리셋부터 매니저 설정까지 다 뒤진 다음에야 여기에 도달하게 된다.
	if (Target == nullptr)
	{
		UE_LOG(LogTRGameplay, Warning,
			TEXT("[Environment] 레벨에 %s 가 없다. %s 갱신을 건너뛴다."), ActorLabel, SystemLabel);
	}
}

//레벨에서 대상 액터를 찾는다
void UTREnvironmentBlenderComponent::ResolveTargets()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	ResolveEnvironmentTarget(*World, bBlendFog, FogActor,
		TEXT("ExponentialHeightFog"), TEXT("안개"));
	ResolveEnvironmentTarget(*World, bBlendSun, SunActor,
		TEXT("DirectionalLight"), TEXT("해"));
	ResolveEnvironmentTarget(*World, bBlendSkyLight, SkyLightActor,
		TEXT("SkyLight"), TEXT("하늘빛"));

	if (SunActor)
	{
		WarnIfNotMovable(SunActor->GetLightComponent(), TEXT("DirectionalLight"));
	}

	if (SkyLightActor)
	{
		WarnIfNotMovable(SkyLightActor->GetLightComponent(), TEXT("SkyLight"));
	}
}

//Static 라이트는 런타임 변경을 조용히 버린다
void UTREnvironmentBlenderComponent::WarnIfNotMovable(
	const USceneComponent* LightComponent, const TCHAR* Label) const
{
	if (LightComponent == nullptr || LightComponent->Mobility == EComponentMobility::Movable)
	{
		return;
	}

	UE_LOG(LogTRGameplay, Warning,
		TEXT("[Environment] %s 의 Mobility 가 Movable 이 아니다(%s). 엔진이 런타임 변경을 "
			 "조용히 버리므로 이 라이트는 바이옴을 따라가지 않는다."),
		Label, *UEnum::GetValueAsString(LightComponent->Mobility.GetValue()));
}

//두 프리셋을 섞어 레벨에 적용한다
void UTREnvironmentBlenderComponent::ApplyBlend(
	const UTRBiomeEnvironment* From, const UTRBiomeEnvironment* To, float Alpha)
{
	// 환경을 정의하지 않은 바이옴은 "레벨 기본값을 그대로 둔다"는 뜻이다. 억지로 0 을
	// 밀어 넣으면 안개가 사라지고 해가 꺼져서, 설정을 빠뜨린 것이 사고처럼 보인다.
	if (To == nullptr)
	{
		return;
	}

	if (From == nullptr)
	{
		From = To;
		Alpha = 1.f;
	}

	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

	if (bBlendFog && FogActor)
	{
		if (UExponentialHeightFogComponent* Fog = FogActor->GetComponent())
		{
			const FTRFogEnvironment Value = FTRFogEnvironment::Lerp(From->Fog, To->Fog, Alpha);
			Fog->SetFogDensity(Value.FogDensity);
			Fog->SetFogHeightFalloff(Value.FogHeightFalloff);
			Fog->SetFogInscatteringColor(Value.FogInscatteringLuminance);
			Fog->SetFogMaxOpacity(Value.FogMaxOpacity);
			Fog->SetStartDistance(Value.StartDistance);
			Fog->SetVolumetricFogExtinctionScale(Value.VolumetricFogExtinctionScale);
		}
	}

	if (bBlendSun && SunActor)
	{
		if (ULightComponent* Light = SunActor->GetLightComponent())
		{
			const FTRSunEnvironment Value = FTRSunEnvironment::Lerp(From->Sun, To->Sun, Alpha);
			Light->SetIntensity(Value.Intensity);
			Light->SetLightColor(Value.LightColor);

			// 라이트 컴포넌트가 아니라 액터를 돌린다. 컴포넌트만 돌리면 에디터에서 보이는
			// 액터의 화살표와 실제 빛 방향이 어긋나서, 나중에 각도를 맞출 때 헷갈린다.
			SunActor->SetActorRotation(Value.Rotation);
		}
	}

	if (bBlendSkyLight && SkyLightActor)
	{
		if (USkyLightComponent* Sky = SkyLightActor->GetLightComponent())
		{
			const FTRSkyLightEnvironment Value =
				FTRSkyLightEnvironment::Lerp(From->SkyLight, To->SkyLight, Alpha);
			Sky->SetIntensity(Value.Intensity);
			Sky->SetLightColor(Value.LightColor);

			// RecaptureSky 는 부르지 않는다. 매 틱 하늘을 다시 굽는 것이라 프레임이
			// 통째로 흔들린다. 강도와 틴트만으로 충분하고, 하늘 자체를 바꿔야 하면
			// SkyLight 를 Real-time Capture 로 두는 것이 답이다.
		}
	}

	if (bBlendPostProcess && PostProcessFrom && PostProcessTo)
	{
		// 프리셋이 바뀔 때만 복사한다. FPostProcessSettings 는 1KB 가 넘는다.
		if (AppliedPostProcessFrom != From)
		{
			PostProcessFrom->Settings = From->PostProcess;
			AppliedPostProcessFrom = From;
		}

		if (AppliedPostProcessTo != To)
		{
			PostProcessTo->Settings = To->PostProcess;
			AppliedPostProcessTo = To;
		}

		// 바탕은 늘 온전히 깔고, 그 위에 목표를 Alpha 만큼 얹는다. 섞는 일은 엔진이 한다.
		PostProcessFrom->BlendWeight = 1.f;
		PostProcessTo->BlendWeight = Alpha;
	}
}

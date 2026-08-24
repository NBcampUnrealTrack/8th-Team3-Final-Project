// TRBiomeEnvironment.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"
#include "TRBiomeEnvironment.generated.h"

/**
 * 지면 안개(ExponentialHeightFog) 설정 한 벌.
 *
 * 이름은 UExponentialHeightFogComponent 의 프로퍼티와 일부러 같게 두었다. 에디터에서
 * 안개 액터를 보며 값을 맞춰 보고 그대로 옮겨 적기 위해서다.
 */
USTRUCT(BlueprintType)
struct TR_API FTRFogEnvironment
{
	GENERATED_BODY()

	/** 안개 농도. 체감이 가장 큰 값이다. 0.02 가 엔진 기본. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float FogDensity = 0.02f;

	/** 높이에 따라 얼마나 빨리 옅어지는가. 작을수록 위까지 자욱하다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog", meta = (ClampMin = "0.0"))
	float FogHeightFalloff = 0.2f;

	/** 안개 색. 사막의 누런 먼지와 설원의 푸른 기운이 여기서 갈린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog")
	FLinearColor FogInscatteringLuminance = FLinearColor(0.4f, 0.45f, 0.55f, 1.f);

	/** 안개가 덮을 수 있는 최대치. 1 이면 먼 곳이 완전히 안개에 묻힌다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FogMaxOpacity = 1.f;

	/** 카메라에서 이만큼 떨어진 뒤부터 안개가 낀다(cm). 기차 주변을 맑게 둘 때 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float StartDistance = 0.f;

	/**
	 * 볼류메트릭 안개의 짙기 배수.
	 *
	 * **안개 액터에서 Volumetric Fog 를 켜 두어야 의미가 있다.** 꺼져 있으면 이 값은
	 * 아무 일도 하지 않는다 — 눈보라나 모래폭풍처럼 빛줄기가 보여야 하는 구간에서만 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog", meta = (ClampMin = "0.0"))
	float VolumetricFogExtinctionScale = 1.f;

	static FTRFogEnvironment Lerp(const FTRFogEnvironment& A, const FTRFogEnvironment& B, float Alpha)
	{
		FTRFogEnvironment Result;
		Result.FogDensity = FMath::Lerp(A.FogDensity, B.FogDensity, Alpha);
		Result.FogHeightFalloff = FMath::Lerp(A.FogHeightFalloff, B.FogHeightFalloff, Alpha);
		Result.FogInscatteringLuminance =
			FMath::Lerp(A.FogInscatteringLuminance, B.FogInscatteringLuminance, Alpha);
		Result.FogMaxOpacity = FMath::Lerp(A.FogMaxOpacity, B.FogMaxOpacity, Alpha);
		Result.StartDistance = FMath::Lerp(A.StartDistance, B.StartDistance, Alpha);
		Result.VolumetricFogExtinctionScale =
			FMath::Lerp(A.VolumetricFogExtinctionScale, B.VolumetricFogExtinctionScale, Alpha);
		return Result;
	}
};

/**
 * 태양(DirectionalLight) 설정 한 벌.
 *
 * **이 라이트의 Mobility 는 Movable 이어야 한다.** Static 이면 엔진이 런타임 변경을
 * 조용히 버린다(USkyLightComponent/ULightComponent 의 AreDynamicDataChangesAllowed).
 * 에러도 경고도 없이 해만 안 움직이므로, 블렌더가 시작할 때 한 번 확인해서 경고를 낸다.
 */
USTRUCT(BlueprintType)
struct TR_API FTRSunEnvironment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun")
	FLinearColor LightColor = FLinearColor::White;

	/** 밝기(lux). DirectionalLight 기본값이 10 이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun", meta = (ClampMin = "0.0"))
	float Intensity = 10.f;

	/**
	 * 해의 방향. Pitch 가 고도(-90 이 머리 위), Yaw 가 방위다.
	 *
	 * 바이옴마다 해 높이를 다르게 두면 시간대가 다른 곳으로 넘어간 느낌이 난다. 다만
	 * 각도를 크게 흔들면 그림자가 화면을 쓸고 지나가므로, 전환 폭은 좁게 잡는 편이 낫다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun")
	FRotator Rotation = FRotator(-45.f, 0.f, 0.f);

	static FTRSunEnvironment Lerp(const FTRSunEnvironment& A, const FTRSunEnvironment& B, float Alpha)
	{
		FTRSunEnvironment Result;
		Result.LightColor = FMath::Lerp(A.LightColor, B.LightColor, Alpha);
		Result.Intensity = FMath::Lerp(A.Intensity, B.Intensity, Alpha);

		// 오일러 각을 그대로 섞으면 359도에서 1도로 갈 때 해가 한 바퀴 돈다.
		// 쿼터니언으로 섞어서 늘 짧은 쪽으로 돌게 한다.
		Result.Rotation = FQuat::Slerp(A.Rotation.Quaternion(), B.Rotation.Quaternion(), Alpha).Rotator();
		return Result;
	}
};

/** 하늘빛(SkyLight) 설정 한 벌. 그늘의 색과 밝기를 정한다. */
USTRUCT(BlueprintType)
struct TR_API FTRSkyLightEnvironment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky Light")
	FLinearColor LightColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky Light", meta = (ClampMin = "0.0"))
	float Intensity = 1.f;

	static FTRSkyLightEnvironment Lerp(const FTRSkyLightEnvironment& A, const FTRSkyLightEnvironment& B, float Alpha)
	{
		FTRSkyLightEnvironment Result;
		Result.LightColor = FMath::Lerp(A.LightColor, B.LightColor, Alpha);
		Result.Intensity = FMath::Lerp(A.Intensity, B.Intensity, Alpha);
		return Result;
	}
};

/**
 * 한 풍경의 환경 설정 한 벌. 바이옴 하나에 애셋 하나.
 *
 * **레벨이 아니라 애셋에 두는 이유가 있다.** 이 값들을 매니저 액터에 직접 넣으면
 * 바이옴 설정이 .umap 안에 들어간다. 그러면 두 사람이 서로 다른 바이옴을 손보기만 해도
 * 바이너리 맵 파일에서 충돌하고, 맵은 병합이 안 되므로 한쪽 작업이 통째로 날아간다.
 * 애셋으로 쪼개면 각자 다른 파일을 건드리게 되고, 레벨을 새로 만들 때 설정을 복붙할
 * 일도 없다.
 *
 * 포스트프로세스만 개별 값이 아니라 구조체 통째로 들고 있다. FPostProcessSettings 는
 * 필드가 200 개가 넘어서 손으로 섞으면 반드시 빠뜨린다. 대신 블렌더가 언바운드
 * 포스트프로세스 컴포넌트 두 개에 이 구조체를 그대로 얹고 BlendWeight 만 움직인다 —
 * 섞는 일은 엔진이 한다.
 */
UCLASS(BlueprintType)
class TR_API UTRBiomeEnvironment : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
	FTRFogEnvironment Fog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
	FTRSunEnvironment Sun;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
	FTRSkyLightEnvironment SkyLight;

	/**
	 * 이 풍경의 포스트프로세스. 노출·색보정·비네트 등 볼륨에서 만지던 것 전부.
	 *
	 * 켠 항목(체크박스)만 적용된다. 안 켠 항목은 레벨에 원래 있던 볼륨 값이 그대로
	 * 비쳐 보인다 — 바이옴이 색만 만지고 노출은 안 건드리게 두고 싶을 때 그렇게 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
	FPostProcessSettings PostProcess;

	/**
	 * 경계선 없이 바뀔 때 이 풍경으로 넘어오는 데 걸리는 시간(초).
	 *
	 * **보통은 안 쓰인다.** 지면 전환이 있는 구간에서는 경계선이 기차를 지나는 진행도를
	 * 그대로 따라가므로, 하늘과 땅이 정의상 어긋날 수 없다. 이 값은 지면 텍스처를 안
	 * 바꾸는 구간(같은 땅에 비가 오기 시작하는 식)처럼 따라갈 선이 없을 때만 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float BlendSeconds = 4.f;
};

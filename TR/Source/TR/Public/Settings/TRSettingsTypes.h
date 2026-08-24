// TRSettingsTypes.h

#pragma once

#include "CoreMinimal.h"
#include "TRSettingsTypes.generated.h"

// 개별 Scalability 설정 대상
UENUM(BlueprintType)
enum class ETRGraphicsQuality : uint8
{
	ViewDistance       UMETA(DisplayName = "View Distance"),
	AntiAliasing       UMETA(DisplayName = "Anti-Aliasing"),
	Shadow             UMETA(DisplayName = "Shadow"),
	GlobalIllumination UMETA(DisplayName = "Global Illumination"),
	Reflection         UMETA(DisplayName = "Reflection"),
	PostProcess        UMETA(DisplayName = "Post Process"),
	Texture            UMETA(DisplayName = "Texture"),
	Effects            UMETA(DisplayName = "Effects"),
	Foliage            UMETA(DisplayName = "Foliage"),
	Shading            UMETA(DisplayName = "Shading"),
	Landscape          UMETA(DisplayName = "Landscape")
};

// Scalability 품질 단계. UGameUserSettings의 0~4 값과 일치
UENUM(BlueprintType)
enum class ETRQualityLevel : uint8
{
	Low       = 0 UMETA(DisplayName = "Low"),
	Medium    = 1 UMETA(DisplayName = "Medium"),
	High      = 2 UMETA(DisplayName = "High"),
	Epic      = 3 UMETA(DisplayName = "Epic"),
	Cinematic = 4 UMETA(DisplayName = "Cinematic")
};

// 설정 행과 메인 설정 화면 사이에서 사용하는 타입 안전 식별자
UENUM(BlueprintType)
enum class ETRSettingId : uint8
{
	None,
	DisplayMode,
	DisplayMonitor,
	Resolution,
	ResolutionScale,
	VSync,
	DynamicResolution,
	FrameRate,
	OverallQuality,
	ViewDistanceQuality,
	AntiAliasingQuality,
	ShadowQuality,
	GlobalIlluminationQuality,
	ReflectionQuality,
	PostProcessQuality,
	TextureQuality,
	EffectsQuality,
	FoliageQuality,
	ShadingQuality,
	LandscapeQuality,
	MasterVolume,
	MusicVolume,
	EffectsVolume,
	LookSensitivity
};

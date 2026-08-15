// TRTypes.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/EngineTypes.h"
#include "TRTypes.generated.h"

#pragma region Character

UENUM(BlueprintType)
enum class ETRActionType : uint8
{
	None,
	Repair,
	Build,
	Reload,
	Restock
};


USTRUCT(BlueprintType)
struct FTRActionDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float CycleDuration = 5.f;  // 1사이클 소요
	
	UPROPERTY(EditAnywhere)
	int32 ResourceAmount = 1;   // 사이클당 필요한 자원
	
	UPROPERTY(EditAnywhere)
	FText Prompt;				// 미사용
};
#pragma endregion


#pragma region Train

namespace TRCollisionChannel
{
	// 적 → 기차 피해 판정 전용. DefaultEngine.ini 의 채널 등록 순서와 인덱스가 일치해야 한다
	// (ATRTrain::BeginPlay 에서 ensure 로 검증).
	constexpr ECollisionChannel Damage = ECC_GameTraceChannel1;
	constexpr const TCHAR* DamageName = TEXT("TRDamage");

	// 판 시작 전 상호작용 트레이스 전용. UTRInteractionComponent 가 Visibility 대신 이 채널로 쏜다
	constexpr ECollisionChannel LobbyInteract = ECC_GameTraceChannel2;
	constexpr const TCHAR* LobbyInteractName = TEXT("LobbyInteract");
}

// DefaultEngine.ini 의 +Profiles 에 등록된 이름과 일치해야 한다.
namespace TRCollisionProfile
{
	// TRDamage 트레이스에만 반응. 장갑 DamageVolume 용.
	constexpr const TCHAR* ArmorDamageVolume = TEXT("TRDamageVolume");

	// Visibility 만 Block — 상호작용 트레이스에 걸리되 이동은 막지 않는다.
	constexpr const TCHAR* Interactable = TEXT("TRInteractable");

	// Visibility 만 Ignore — 조준선에 안 걸려 실내 구조물로만 상호작용된다.
	constexpr const TCHAR* ExteriorPart = TEXT("TRExteriorPart");

	// LobbyInteract 만 Block — 로비에서만 조준선에 걸린다. 로비에서만 상호작용 하는 Actor들 전용.
	constexpr const TCHAR* LobbyInteractable = TEXT("TRLobbyInteractable");

	// Visibility·LobbyInteract 둘 다 Block — 로비와 게임 양쪽에서 잡힌다. 관제석 등.
	constexpr const TCHAR* DualInteractable = TEXT("TRDualInteractable");
}

// ATRTrain 의 슬롯 맵 키. 값을 추가하면 레벨에서 해당 칸도 채워야 한다 (BeginPlay 에서 ensure).
UENUM(BlueprintType)
enum class ETRArmorSlot : uint8
{
	Car1Left,
	Car1Right,
	Car2Left,
	Car2Right
};

UENUM(BlueprintType)
enum class ETRTurretSlot : uint8
{
	Car1Left,
	Car1Right,
	Car2Left,
	Car2Right,
	Car2Rear
};

USTRUCT(BlueprintType)
struct FTRArmorLevelRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float MaxHP = 0.f;

	UPROPERTY(EditAnywhere)
	int32 InstallCost = 0;   // 재설치 비용

	UPROPERTY(EditAnywhere)
	int32 UpgradeCost = 0;   // 이 레벨에서 '다음' 레벨로 올리는 비용. 최대 레벨 행은 0.
};

USTRUCT(BlueprintType)
struct FTRTurretLevelRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float MaxHP = 0.f;

	UPROPERTY(EditAnywhere)
	int32 InstallCost = 0;   // 재설치 비용

	UPROPERTY(EditAnywhere)
	int32 UpgradeCost = 0;   // 이 레벨에서 다음 레벨로 올리는 비용. 최대 레벨 행은 0.

	UPROPERTY(EditAnywhere)
	float ManualRange = 0.f;

	UPROPERTY(EditAnywhere)
	float ManualDamage = 0.f;

	UPROPERTY(EditAnywhere)
	float ManualFireRate = 0.f;

	UPROPERTY(EditAnywhere)
	float AutoRange = 0.f;

	UPROPERTY(EditAnywhere)
	float AutoDamagePerShot = 0.f;   // 발당 피해

	UPROPERTY(EditAnywhere)
	float AutoFireRate = 0.f;

	UPROPERTY(EditAnywhere)
	int32 AutoMagazine = 0;
};
#pragma endregion

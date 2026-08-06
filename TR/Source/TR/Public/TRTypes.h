// TRTypes.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
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
	float CycleDuration = 5.f;   // 1사이클 소요
	
	UPROPERTY(EditAnywhere)
	int32 ResourceAmount = 1;        // 사이클당 필요한 자원
	
	UPROPERTY(EditAnywhere)
	bool bRepeating = true;      // 완료까지 반복?
	
	UPROPERTY(EditAnywhere)
	FText Prompt;
};
#pragma endregion


#pragma region Train

USTRUCT(BlueprintType)
struct FTRArmorLevelRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float MaxHP = 0.f;

	UPROPERTY(EditAnywhere)
	int32 InstallCost = 0;   // 재설치 비용

	UPROPERTY(EditAnywhere)
	int32 UpgradeCost = 0;   // 업그레이드 비용 (관제석용, 나중에 사용)
};

USTRUCT(BlueprintType)
struct FTRTurretLevelRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float MaxHP = 0.f;

	UPROPERTY(EditAnywhere)
	int32 InstallCost = 0;

	UPROPERTY(EditAnywhere)
	int32 UpgradeCost = 0;

	UPROPERTY(EditAnywhere)
	float ManualRange = 0.f;

	UPROPERTY(EditAnywhere)
	float ManualDamage = 0.f;

	UPROPERTY(EditAnywhere)
	int32 ManualAmmoPerResource = 0;   // 물자실 자원 1개당 발수

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

// TRTypes.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TRTypes.generated.h"

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

USTRUCT(BlueprintType)
struct FTRInstallCostRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	int32 Cost = 0;
};

// TRArmor.h

#pragma once

#include "CoreMinimal.h"
#include "Train/TRInstallablePart.h"
#include "TRArmor.generated.h"

class UBoxComponent;
class UDataTable;

UCLASS()
class TR_API ATRArmor : public ATRInstallablePart
{
	GENERATED_BODY()

public:
	ATRArmor();

	virtual float GetMaxHP() const override;
	virtual int32 GetInstallCost() const override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Part")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(EditDefaultsOnly, Category = "Part")
	TObjectPtr<UDataTable> LevelTable;   // FTRArmorLevelRow, RowName = 레벨
};

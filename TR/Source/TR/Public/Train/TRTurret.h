// TRTurret.h

#pragma once

#include "CoreMinimal.h"
#include "Train/TRInstallablePart.h"
#include "TRTurret.generated.h"

class UStaticMeshComponent;
class UDataTable;
class APawn;

UCLASS()
class TR_API ATRTurret : public ATRInstallablePart
{
	GENERATED_BODY()

public:
	ATRTurret();

	virtual float GetMaxHP() const override;
	virtual int32 GetInstallCost() const override;

	UFUNCTION(BlueprintPure, Category = "Turret")
	bool IsOccupied() const { return Occupant != nullptr; }

protected:
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Occupant;

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Turret")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Turret")
	TObjectPtr<UDataTable> LevelTable;   // FTRTurretLevelRow, RowName = 레벨
};

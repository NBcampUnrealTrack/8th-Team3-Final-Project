// TRTurret.h

#pragma once

#include "CoreMinimal.h"
#include "Train/TRInstallablePart.h"
#include "TRTurret.generated.h"

class UStaticMeshComponent;
class UDataTable;
class APawn;
struct FTRTurretLevelRow;

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
	
	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetManualRange() const;

	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetManualDamage() const;

	UFUNCTION(BlueprintPure, Category = "Turret")
	int32 GetManualAmmoPerResource() const;
	
	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetManualFireRate() const;
	
	UFUNCTION(BlueprintPure, Category = "Turret")
	int32 GetMaxAmmo() const;
	
	UFUNCTION(BlueprintPure, Category = "Turret")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }
	
	UStaticMeshComponent* GetTurretMesh() const { return TurretMesh; }
	
	UFUNCTION(BlueprintPure, Category = "Turret")
	bool IsWithinFireArc(float CameraYaw, float CameraPitch) const;
	
	void SetOccupant(APawn* NewOccupant) { Occupant = NewOccupant; }
	
	void ConsumeAmmo(int32 Amount);
	void AddAmmo(int32 Amount);
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Occupant;

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FTRTurretLevelRow* GetCurrentLevelRow(const TCHAR* ContextName) const;

	UPROPERTY(VisibleAnywhere, Category = "Turret")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Turret")
	TObjectPtr<UDataTable> LevelTable;   // FTRTurretLevelRow, RowName = 레벨
	
	UPROPERTY(Replicated)
	int32 CurrentAmmo = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Turret")
	float FireArcHalfAngle = 80.f;
	
	float InstallYaw = 0.f;
	float InstallPitch = 0.f;
};

// TRTurret.h

#pragma once

#include "CoreMinimal.h"
#include "Train/TRInstallablePart.h"
#include "TRTurret.generated.h"

class UStaticMeshComponent;
class USceneComponent;
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

	UFUNCTION(BlueprintPure, Category = "Turret")
	bool CanBeReloaded() const { return !IsPartDestroyed() && GetCurrentAmmo() < GetMaxAmmo(); }

	UFUNCTION(BlueprintPure, Category = "Turret")
	bool CanBeMounted() const { return !IsPartDestroyed() && !IsOccupied(); }
	
	void SetOccupant(APawn* NewOccupant) { Occupant = NewOccupant; }
	
	void ConsumeAmmo(int32 Amount);
	void AddAmmo(int32 Amount);
	
protected:
	virtual void BeginPlay() override;
	
	virtual void OnPartDestroyed() override;
	
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Occupant;

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FTRTurretLevelRow* GetCurrentLevelRow(const TCHAR* ContextName) const;

	UPROPERTY(VisibleAnywhere, Category = "Turret")
	TObjectPtr<USceneComponent> Root;

	// 기차 외부. 적의 피격 대상이자 탑승 시 카메라가 붙는 지점.
	UPROPERTY(VisibleAnywhere, Category = "Turret")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	// 기차 내부 조종 구조물. 탑승·수리·재장전은 전부 여기서 상호작용한다.
	UPROPERTY(VisibleAnywhere, Category = "Turret")
	TObjectPtr<UStaticMeshComponent> ConsoleMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Turret")
	TObjectPtr<UDataTable> LevelTable;   // FTRTurretLevelRow, RowName = 레벨
	
	UPROPERTY(Replicated)
	int32 CurrentAmmo = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Turret")
	float FireArcHalfAngle = 80.f;
	
	float InstallYaw = 0.f;
	float InstallPitch = 0.f;
};

// TRTurret.h

#pragma once

#include "CoreMinimal.h"
#include "Train/TRInstallablePart.h"
#include "TRTurret.generated.h"

class UStaticMeshComponent;
class USceneComponent;
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
	float GetManualRange() const;

	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetManualDamage() const;
	
	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetManualFireRate() const;

	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetAutoRange() const;

	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetAutoDamagePerShot() const;

	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetAutoFireRate() const;
	
	UFUNCTION(BlueprintPure, Category = "Turret")
	int32 GetMaxAmmo() const;
	
	UFUNCTION(BlueprintPure, Category = "Turret")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }
	
	UStaticMeshComponent* GetTurretMesh() const { return TurretMesh; }
	
	UFUNCTION(BlueprintPure, Category = "Turret")
	bool IsOccupied() const { return Occupant != nullptr; }
	
	UFUNCTION(BlueprintPure, Category = "Turret")
	bool IsWithinFireArc(float CameraYaw, float CameraPitch) const;

	// 사각의 중심(설치 시 메시가 향한 방향)과 반각.
	// 탑승 카메라 클램프가 이 값들로 시야를 묶어, 서버 검사와 같은 범위를 쓴다.
	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetInstallYaw() const { return InstallYaw; }

	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetInstallPitch() const { return InstallPitch; }

	UFUNCTION(BlueprintPure, Category = "Turret")
	float GetFireArcHalfAngle() const { return FireArcHalfAngle; }

	UFUNCTION(BlueprintPure, Category = "Turret")
	bool CanBeReloaded() const { return !IsPartDestroyed() && GetCurrentAmmo() < GetMaxAmmo(); }

	UFUNCTION(BlueprintPure, Category = "Turret")
	bool CanBeMounted() const { return !IsPartDestroyed() && !IsOccupied(); }
	
	void SetOccupant(APawn* NewOccupant);
	
	void ConsumeAmmo(int32 Amount);
	
	// 실제로 채웠으면 true. 호출부는 이 값으로 자원 차감 여부를 정한다.
	bool RefillAmmoToMax();

	// 사격 연출을 전원에게 알린다. 연출을 위한 코드.
	// 자동·수동 모두 이 함수를 쓴다.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireEffect(FVector HitLocation, bool bEnemyHit);
	void MulticastFireEffect_Implementation(FVector HitLocation, bool bEnemyHit);
	
protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnPartDestroyed() override;
	
	virtual void OnPartInstalled() override;
	
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Occupant;

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 레벨별 수치는 ATRGameState 가 테이블을 들고 답한다 — 값이 모든 포탑에 공통이라서다.
	const FTRTurretLevelRow* GetCurrentLevelRow(const TCHAR* ContextName) const;

	// 레벨 업그레이드 알림. AutoFireRate 가 바뀐 만큼 타이머 주기를 다시 잡는다.
	void HandleLevelUpgraded(float PrevMaxHP);

	// 사거리·사격각 안에서 원점에 가장 가까운 적의 위치. 없으면 false.
	// MassNavigation 해시 그리드로 후보를 좁힌 뒤 거리·각도로 고른다.
	bool FindNearestEnemy(const FVector& Origin, float Range, FVector& OutTargetLocation) const;

	// 무인 상태에서 주기적으로 자동 사격한다. 주기는 AutoFireRate 가 정한다.
	void StartAutoFire();
	void StopAutoFire();
	void AutoFireTick();

	FTimerHandle AutoFireTimerHandle;
	
	UPROPERTY(VisibleAnywhere, Category = "Turret")
	TObjectPtr<USceneComponent> Root;

	// 기차 외부. 적의 피격 대상이자 탑승 시 카메라가 붙는 지점.
	UPROPERTY(VisibleAnywhere, Category = "Turret")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	// 기차 내부 조종 구조물. 탑승·수리·재장전은 전부 여기서 상호작용한다.
	UPROPERTY(VisibleAnywhere, Category = "Turret")
	TObjectPtr<UStaticMeshComponent> ConsoleMesh;

	UPROPERTY(Replicated)
	int32 CurrentAmmo = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Turret")
	float FireArcHalfAngle = 80.f;
	
	float InstallYaw = 0.f;
	float InstallPitch = 0.f;
};

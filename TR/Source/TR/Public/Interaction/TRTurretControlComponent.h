// TRTurretControlComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TRTurretControlComponent.generated.h"

class APlayerCameraManager;
class ATRTurret;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TR_API UTRTurretControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UTRTurretControlComponent();

	UFUNCTION(BlueprintPure, Category="Turret")
	bool IsMounted() const { return MountedTurret != nullptr; }

	UFUNCTION(BlueprintPure, Category="Turret")
	ATRTurret* GetMountedTurret() const { return MountedTurret; }

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="Turret")
	void ServerMount(ATRTurret* Turret);
	bool ServerMount_Validate(ATRTurret* Turret);
	void ServerMount_Implementation(ATRTurret* Turret);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Turret")
	void ServerDismount();
	void ServerDismount_Implementation();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Turret")
	void ServerFire();
	void ServerFire_Implementation();

	void ForceDismount();
	
private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_MountedTurret();
	
	void CompleteMount();

	// 원점·방향을 호출부에서 받는다. ServerFire 의 적 판정과 같은 값을 써야 두 판정이 어긋나지 않는다.
	bool TraceFireTarget(const FVector& Start, const FVector& Direction, FHitResult& OutHit) const;
	bool TryAutoRefillFromInventory();

	// 로컬 플레이어의 카메라 매니저. 서버/원격 클라이언트에서는 nullptr.
	APlayerCameraManager* GetLocalCameraManager() const;

	// 탑승 중 시야를 포탑 사각 안으로 묶고, 하차 시 원래 값으로 되돌린다.
	void ApplyMountedViewLimits(const ATRTurret* Turret);
	void ClearMountedViewLimits();

	UPROPERTY(ReplicatedUsing=OnRep_MountedTurret)
	TObjectPtr<ATRTurret> MountedTurret = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category="Turret")
	float MountDelay = 0.5f;

	float LastFireTime = -1.f;

	// 탑승 직전의 카메라 각도 제한. 하차 시 그대로 복원한다 —
	// 상수로 되돌리면 다른 시스템이 걸어둔 제한을 덮어쓰게 된다.
	float SavedViewYawMin = 0.f;
	float SavedViewYawMax = 0.f;
	float SavedViewPitchMin = 0.f;
	float SavedViewPitchMax = 0.f;
	bool bViewLimitsSaved = false;

	FTimerHandle MountTimerHandle;

	TWeakObjectPtr<ATRTurret> PendingMountTurret;
};

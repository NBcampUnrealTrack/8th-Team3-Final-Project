// TRTurretControlComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TRTurretControlComponent.generated.h"

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

	bool TraceFireTarget(FHitResult& OutHit) const;
	bool TryAutoRefillFromInventory();

	UPROPERTY(ReplicatedUsing=OnRep_MountedTurret)
	TObjectPtr<ATRTurret> MountedTurret = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category="Turret")
	float MountDelay = 0.5f;

	float LastFireTime = -1.f;
	
	FTimerHandle MountTimerHandle;

	TWeakObjectPtr<ATRTurret> PendingMountTurret;
};

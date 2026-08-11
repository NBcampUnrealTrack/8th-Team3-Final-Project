// TRTrain.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRTrain.generated.h"

class ATRArmor;
class ATRTurret;
class UStaticMeshComponent;

UCLASS()
class TR_API ATRTrain : public AActor
{
	GENERATED_BODY()
public:
	ATRTrain();

	UFUNCTION(BlueprintPure, Category = "Train")
	bool IsDestroyed() const { return CurrentHullHealth <= 0.f; }

	UFUNCTION(BlueprintPure, Category = "Train")
	int32 GetActiveArmorCount() const;

	UFUNCTION(BlueprintPure, Category = "Train")
	bool HasActiveArmor() const { return GetActiveArmorCount() > 0; }

	UFUNCTION(BlueprintPure, Category = "Train")
	float GetMaxHullHealth() const { return MaxHullHealth; }

	UFUNCTION(BlueprintPure, Category = "Train")
	bool IsFullHP() const { return CurrentHullHealth >= MaxHullHealth; }

	UFUNCTION(BlueprintPure, Category = "Train")
	float GetCurrentHullHealth() const { return CurrentHullHealth; }

	UFUNCTION(BlueprintPure, Category = "Train")
	bool CanBeRepaired() const { return !IsDestroyed() && !IsFullHP(); }

	UFUNCTION(BlueprintPure, Category = "Train")
	TArray<ATRArmor*> GetDestroyedArmor() const;

	UFUNCTION(BlueprintPure, Category = "Train")
	TArray<ATRTurret*> GetDestroyedTurrets() const;
	
	void ApplyRepair(float Amount);
	
private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Train")
	TObjectPtr<UStaticMeshComponent> MeshComp;
	
	UPROPERTY(EditDefaultsOnly, Category = "Train")
	float MaxHullHealth = 2000.f;

	// 테스트용 — 데미지 시스템(적 등) 생기면 EditInstanceOnly 제거 검토
	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category = "Train", meta = (AllowPrivateAccess = "true"))
	float CurrentHullHealth = 2000.f;

	UPROPERTY(EditInstanceOnly, Category = "Train")
	TArray<TObjectPtr<ATRArmor>> ArmorSlots;  // 4칸, 레벨에서 각 슬롯에 배치된 장갑 액터 할당

	UPROPERTY(EditInstanceOnly, Category = "Train")
	TArray<TObjectPtr<ATRTurret>> TurretSlots;  // 5칸, 레벨에서 각 슬롯에 배치된 포탑 액터 할당
};

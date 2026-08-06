// TRTrain.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRTrain.generated.h"

class ATRArmor;
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
	
	UPROPERTY(EditDefaultsOnly, Category = "Train")
	float MaxHullHealth = 2000.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Train")
	float CurrentHullHealth = 2000.f;

	UPROPERTY(EditInstanceOnly, Category = "Train")
	TArray<TObjectPtr<ATRArmor>> ArmorSlots; // 4칸, 레벨에서 각 슬롯에 배치된 장갑 액터 할당

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Train")
	TObjectPtr<UStaticMeshComponent> MeshComp;
};

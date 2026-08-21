// TRTrain.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRTypes.h"
#include "TRTrain.generated.h"

class ATRArmor;
class ATRTurret;
class ATRInstallablePart;
class UTRRepairTargetComponent;

UCLASS()
class TR_API ATRTrain : public AActor
{
	GENERATED_BODY()
public:
	ATRTrain();

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// 선체 HP 0 = 게임 오버. 기차 액터 자체는 소멸되지 않는다.
	UFUNCTION(BlueprintPure, Category = "Train")
	bool IsHullDestroyed() const { return CurrentHullHealth <= 0.f; }

	UFUNCTION(BlueprintPure, Category = "Train")
	int32 GetActiveArmorCount() const;

	// 자폭·충돌처럼 한 지점의 충격이 적용될 때의 진입점.
	// 착탄점에 가장 가까운 살아있는 부위(포탑 또는 장갑) 하나가 전부 받고, 그 부위가 파괴됐으면 선체가 받는다
	// 거리 상한은 두지 않는다. 호출자가 이 기차를 목표로 확정한 착탄점만 보낸다.
	void ApplyImpactDamage(const FVector& ImpactPoint, float Damage, AController* EventInstigator, AActor* DamageCauser);

	UFUNCTION(BlueprintPure, Category = "Train")
	bool HasActiveArmor() const { return GetActiveArmorCount() > 0; }

	UFUNCTION(BlueprintPure, Category = "Train")
	float GetMaxHullHealth() const { return MaxHullHealth; }

	UFUNCTION(BlueprintPure, Category = "Train")
	bool IsFullHP() const { return CurrentHullHealth >= MaxHullHealth; }

	UFUNCTION(BlueprintPure, Category = "Train")
	float GetCurrentHullHealth() const { return CurrentHullHealth; }

	UFUNCTION(BlueprintPure, Category = "Train")
	bool CanBeRepaired() const { return !IsHullDestroyed() && !IsFullHP(); }

	UFUNCTION(BlueprintPure, Category = "Train")
	TArray<ATRArmor*> GetDestroyedArmor() const;

	UFUNCTION(BlueprintPure, Category = "Train")
	TArray<ATRTurret*> GetDestroyedTurrets() const;

	// 관제석 UI 진입점 — 위젯 칸마다 슬롯을 지정해 그 자리의 파츠 상태를 읽는다.
	// 슬롯이 비어 있으면 nullptr (레벨에서 할당 누락).
	UFUNCTION(BlueprintPure, Category = "Train")
	ATRArmor* GetArmorAt(ETRArmorSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Train")
	ATRTurret* GetTurretAt(ETRTurretSlot Slot) const;

	void ApplyRepair(float Amount);

protected:
	virtual void BeginPlay() override;

	// 선체 HP가 0이 된 순간 서버에서 1회 호출되는 알림 훅. 기차를 소멸시키지 않는다.
	virtual void OnHullDestroyed();

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 모든 슬롯이 레벨에서 채워졌는지 확인. 비어 있으면 관제석 UI 칸이 조용히 빈 채로 남는다.
	void ValidateSlots() const;
	
	UPROPERTY(VisibleAnywhere, Category = "Train")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTRRepairTargetComponent> RepairTarget;
	
	UPROPERTY(EditDefaultsOnly, Category = "Train")
	float MaxHullHealth = 2000.f;

	// 테스트용 — 데미지 시스템(적 등) 생기면 EditInstanceOnly 제거 검토
	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category = "Train", meta = (AllowPrivateAccess = "true"))
	float CurrentHullHealth = 2000.f;

	// 레벨에서 슬롯마다 배치된 액터를 할당한다.
	// 장갑 하나가 한 면 전체를 담당하며, 창문·피격 볼륨을 여러 개 갖는 건 액터 내부 컴포넌트로 처리한다.
	UPROPERTY(EditInstanceOnly, Category = "Train")
	TMap<ETRArmorSlot, TObjectPtr<ATRArmor>> ArmorSlots;

	UPROPERTY(EditInstanceOnly, Category = "Train")
	TMap<ETRTurretSlot, TObjectPtr<ATRTurret>> TurretSlots;
};

// TRTrain.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRTypes.h"
#include "TRTrain.generated.h"

class ATRArmor;
class ATRTurret;

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

	// 자폭·충돌처럼 한 지점의 충격이 여러 대상에 동시 적용될 때의 진입점.
	// 살아있는 장갑이 그 자리를 덮고 있으면 장갑이 전부 흡수하고, 아니면 폭발에 걸친 포탑들과 선체가 맞는다.
	// Radius 는 터뜨린 쪽의 폭발 반경. 0 이하면 기차 기본값 ImpactRadius 를 쓴다.
	// 착탄점이 기차 근처인지는 검증하지 않는다 — 호출부 책임이다.
	// TODO: 적 구현 시 호출할 것. 현재 호출부가 없어 장갑 흡수 로직 전체가 미검증이다.
	void ApplyImpactDamage(const FVector& ImpactPoint, float Damage, float Radius, AController* EventInstigator, AActor* DamageCauser);

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

	// 해당 지점을 덮는 살아있는 장갑을 반환. 장갑이 없거나 파괴됐으면 nullptr (= 노출 상태).
	ATRArmor* FindArmorAt(const FVector& Point) const;

	// 모든 슬롯이 레벨에서 채워졌는지 확인. 비어 있으면 관제석 UI 칸이 조용히 빈 채로 남는다.
	void ValidateSlots() const;

	UPROPERTY(VisibleAnywhere, Category = "Train")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(EditDefaultsOnly, Category = "Train")
	float MaxHullHealth = 2000.f;

	// 폭발 반경 기본값 (기획: 기차 표면 3m).
	// ApplyImpactDamage 가 Radius 를 못 받았을 때만 쓰인다 — 폭발 크기는 원래 터뜨린 쪽이 정한다.
	UPROPERTY(EditDefaultsOnly, Category = "Train", meta = (ClampMin = "0.0", Units = "cm"))
	float ImpactRadius = 300.f;

	// 테스트용 — 데미지 시스템(적 등) 생기면 EditInstanceOnly 제거 검토
	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category = "Train", meta = (AllowPrivateAccess = "true"))
	float CurrentHullHealth = 2000.f;

	// 레벨에서 슬롯마다 배치된 액터를 할당한다. 맵이라 한 슬롯에 둘이 들어갈 수 없다.
	// 장갑 하나가 한 면 전체를 담당하며, 창문·피격 볼륨을 여러 개 갖는 건 액터 내부 컴포넌트로 처리한다.
	UPROPERTY(EditInstanceOnly, Category = "Train")
	TMap<ETRArmorSlot, TObjectPtr<ATRArmor>> ArmorSlots;

	UPROPERTY(EditInstanceOnly, Category = "Train")
	TMap<ETRTurretSlot, TObjectPtr<ATRTurret>> TurretSlots;
};

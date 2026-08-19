// TRInstallablePart.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRInstallablePart.generated.h"

class UNiagaraComponent;
class USceneComponent;

UCLASS(Abstract)
class TR_API ATRInstallablePart : public AActor
{
	GENERATED_BODY()

public:
	ATRInstallablePart();
	
	// 레벨별 수치는 자식이 ATRGameState 의 DataTable을 조회해 답변
	virtual float GetMaxHP() const PURE_VIRTUAL(ATRInstallablePart::GetMaxHP, return 0.f;);
	
	virtual int32 GetInstallCost() const PURE_VIRTUAL(ATRInstallablePart::GetInstallCost, return 0;);
	
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	
	// HP 0 상태. 액터는 소멸되지 않고 슬롯에 그대로 남는다 (관제석 재설치 대상).
	UFUNCTION(BlueprintPure, Category = "Part")
	bool IsPartDestroyed() const { return CurrentHP <= 0.f; }

	UFUNCTION(BlueprintPure, Category = "Part")
	bool IsFullHP() const { return CurrentHP >= GetMaxHP(); }

	UFUNCTION(BlueprintPure, Category = "Part")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintPure, Category = "Part")
	bool CanBeRepaired() const { return !IsPartDestroyed() && !IsFullHP(); }

	// 수리 로직 — 공통
	void ApplyRepair(float Amount);

	// 설치 로직 — 공통 (풀피로 초기화)
	void ApplyInstall();

	// 레벨이 올라 최대 HP 가 커졌을 때 현재 HP 비율을 유지한 채 따라 올린다. 서버 전용.
	// ATRGameState 의 레벨 업그레이드 델리게이트에 자식이 직접 건다.
	void ApplyLevelUpgrade(float PrevMaxHP);
	
protected:
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// HP가 0이 된 순간 서버에서 1회 호출되는 알림 훅.
	// 액터를 소멸시키지 않는다. 재설치를 위해 슬롯에 남아야 하므로 Destroy() 호출 금지.
	virtual void OnPartDestroyed();

	// HP 가 최대치로 채워져 설치가 완료된 순간 서버에서 1회 호출되는 알림 훅.
	virtual void OnPartInstalled();

private:
	// 서버에서 체력 임계치를 계산하고 복제 상태를 갱신한다.
	void UpdateDamageEffectState();

	// Niagara 컴포넌트는 복제하지 않고 이 bool만 각 머신의 활성 상태에 반영한다.
	void RefreshDamageEffect();

	UFUNCTION()
	void OnRep_DamageEffectActive();

protected:
	// 파생 파츠의 컴포넌트가 공유하는 기준점.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Part")
	TObjectPtr<USceneComponent> Root;

	//--- VFX ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Part|VFX")
	TObjectPtr<UNiagaraComponent> DamageEffect;

	// 현재 체력이 최대 체력에서 이 비율 이하로 내려가면 손상 지속 이펙트를 켠다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Part|VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageEffectThresholdRatio = 0.5f;

	// 테스트용 — 데미지 시스템(적 등) 생기면 EditInstanceOnly 제거 검토
	UPROPERTY(EditInstanceOnly, Replicated)
	float CurrentHP = 0.f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_DamageEffectActive)
	bool bDamageEffectActive = false;
};

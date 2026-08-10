// TRInstallablePart.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRInstallablePart.generated.h"

UCLASS(Abstract)
class TR_API ATRInstallablePart : public AActor
{
	GENERATED_BODY()

public:
	ATRInstallablePart();
	
	// 레벨별 수치는 자식이 자기 DataTable로 답변
	virtual float GetMaxHP() const PURE_VIRTUAL(ATRInstallablePart::GetMaxHP, return 0.f;);
	
	virtual int32 GetInstallCost() const PURE_VIRTUAL(ATRInstallablePart::GetInstallCost, return 0;);
	
	UFUNCTION(BlueprintPure, Category = "Part")
	bool IsDestroyed() const { return CurrentHP <= 0.f; }

	UFUNCTION(BlueprintPure, Category = "Part")
	bool IsFullHP() const { return CurrentHP >= GetMaxHP(); }

	UFUNCTION(BlueprintPure, Category = "Part")
	float GetCurrentHP() const { return CurrentHP; }
	
	// 수리 로직 — 공통
	void ApplyRepair(float Amount);

	// 설치 로직 — 공통 (풀피로 초기화)
	void ApplyInstall();
	
protected:
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 테스트용 — 데미지 시스템(적 등) 생기면 EditInstanceOnly 제거 검토
	UPROPERTY(EditInstanceOnly, Replicated)
	float CurrentHP = 0.f;
};

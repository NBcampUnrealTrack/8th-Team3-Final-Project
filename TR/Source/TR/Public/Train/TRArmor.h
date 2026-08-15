// TRArmor.h

#pragma once

#include "CoreMinimal.h"
#include "Train/TRInstallablePart.h"
#include "TRArmor.generated.h"

class UBoxComponent;
class USceneComponent;
struct FTRArmorLevelRow;

UCLASS()
class TR_API ATRArmor : public ATRInstallablePart
{
	GENERATED_BODY()

public:
	ATRArmor();

	virtual float GetMaxHP() const override;
	virtual int32 GetInstallCost() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 레벨별 수치는 ATRGameState 가 테이블을 들고 답한다
	const FTRArmorLevelRow* GetCurrentLevelRow(const TCHAR* ContextName) const;

	// 형태 없는 기준점. CollisionBox 와 DamageVolume 은 크기·위치가 전혀 다르므로
	// 한쪽이 다른 쪽에 딸려 움직이지 않도록 둘 다 이 루트에 형제로 붙인다.
	UPROPERTY(VisibleAnywhere, Category = "Part")
	TObjectPtr<USceneComponent> Root;

	// 창문 장갑 — 실내 상호작용용
	UPROPERTY(VisibleAnywhere, Category = "Part")
	TObjectPtr<UBoxComponent> CollisionBox;

	// 외부 피격 판정 영역. 이 장갑이 지키는 구역을 덮도록 레벨에서 크기·위치를 잡는다.
	// 눈에 보이지 않으며 상호작용 트레이스에도 걸리지 않는다.
	UPROPERTY(VisibleAnywhere, Category = "Part")
	TObjectPtr<UBoxComponent> DamageVolume;
};

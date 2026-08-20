// TRArmor.h

#pragma once

#include "CoreMinimal.h"
#include "Train/TRInstallablePart.h"
#include "TRArmor.generated.h"

class UBoxComponent;
struct FTRArmorLevelRow;

UCLASS()
class TR_API ATRArmor : public ATRInstallablePart
{
	GENERATED_BODY()

public:
	ATRArmor();

	virtual float GetMaxHP() const override;
	virtual int32 GetInstallCost() const override;

	UBoxComponent* GetDamageVolume() const { return DamageVolume; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 레벨별 수치는 ATRGameState 가 테이블을 들고 답한다
	const FTRArmorLevelRow* GetCurrentLevelRow(const TCHAR* ContextName) const;
	
	// 창문 방범창. 실내에서 수리를 상호작용하는 지점. 장갑 한 칸에 창문이 둘이다.
	UPROPERTY(VisibleAnywhere, Category = "Part")
	TObjectPtr<UBoxComponent> WindowCollision1;

	UPROPERTY(VisibleAnywhere, Category = "Part")
	TObjectPtr<UBoxComponent> WindowCollision2;

	// 외부 피격 판정 영역. 이 장갑이 지키는 구역을 덮도록 레벨에서 크기·위치를 잡는다.
	// 눈에 보이지 않으며 상호작용 트레이스에도 걸리지 않는다.
	UPROPERTY(VisibleAnywhere, Category = "Part")
	TObjectPtr<UBoxComponent> DamageVolume;
};

// TRChaserTrait.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Mass/Core/TRMassParams.h"
#include "TRChaserTrait.generated.h"

UCLASS(meta = (DisplayName = "TR Chaser"))
class TR_API UTRChaserTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	//추적 대상
	UPROPERTY(EditAnywhere, Category = "TR|Chase")
	ETRTargetChannel Channel = ETRTargetChannel::Train;

	UPROPERTY(EditAnywhere, Category = "TR|Chase")
	FTRChaseParams ChaseParams;

	/**
	 * 종류와 전투 수치. Config 애셋마다 이 값만 바꾸면 근접형/사출형/사격형이 갈린다.
	 * 정지 거리도 여기서 파생된다(UE::TR::Enemy::MakeApproach).
	 */
	UPROPERTY(EditAnywhere, Category = "TR|Enemy")
	FTREnemyCombatParams CombatParams;
	
	UPROPERTY(EditAnywhere, Category = "TR|Chase", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float AgentRadius = 40.f;
};

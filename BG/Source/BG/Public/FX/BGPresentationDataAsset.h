#pragma once

#include "CoreMinimal.h"
#include "Card/BGCardPresentationTypes.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "BGPresentationDataAsset.generated.h"

class UNiagaraSystem;

/**
 * 카드에 종속되지 않는 공통 연출 자산의 최상위 카탈로그.
 *
 * 아트 작업자는 Content Browser의 DA_BGPresentation을 열어 에셋을 교체한다.
 * 실제 Niagara는 Soft Reference로 유지해 이 DataAsset을 여는 것만으로 전부 로드하지 않는다.
 * 추후 턴·사망·승패 등 시스템 연출은 의미별 Category/구조체로 여기에 확장한다.
 */
UCLASS(BlueprintType)
class BG_API UBGPresentationDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 주사위 실패 시 카드별 Primary FX 대신 재생하는 공통 연출. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Resolution")
	TSoftObjectPtr<UNiagaraSystem> FailureEffect;

	/** 주사위 대실패 시 카드별 Primary FX 대신 재생하는 공통 연출. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Resolution")
	TSoftObjectPtr<UNiagaraSystem> CriticalFailureEffect;

	/** 일반 카드의 대상 선택 선. 다른 계열이 비어 있으면 이 연출로 폴백한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Selection")
	TSoftObjectPtr<UNiagaraSystem> BasicTargetSelectionEffect;

	/** 마법 계열 카드의 대상 선택 선. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Selection")
	TSoftObjectPtr<UNiagaraSystem> MagicTargetSelectionEffect;

	/** 물리/무기 계열 카드의 대상 선택 선. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Selection")
	TSoftObjectPtr<UNiagaraSystem> PhysicalTargetSelectionEffect;

	/** User.bIsEnding 전달 후 컴포넌트를 제거하기까지 기다리는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Selection", meta = (ClampMin = "0.0"))
	float TargetSelectionEndDelaySeconds = 1.f;

	const TSoftObjectPtr<UNiagaraSystem>& GetTargetSelectionEffect(EBGTargetSelectionStyle Style) const
	{
		switch (Style)
		{
		case EBGTargetSelectionStyle::Magic:
			return MagicTargetSelectionEffect;
		case EBGTargetSelectionStyle::Physical:
			return PhysicalTargetSelectionEffect;
		default:
			return BasicTargetSelectionEffect;
		}
	}
};

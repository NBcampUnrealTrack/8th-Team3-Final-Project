#pragma once

#include "CoreMinimal.h"
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
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "BGCardNiagaraTypes.generated.h"

class UNiagaraSystem;

/** 카드 연출 자산을 카드 Row에서 읽을지, 사용자 직업 Row에서 읽을지 결정한다. */
UENUM(BlueprintType)
enum class EBGCardFXSource : uint8
{
	CardSpecific   UMETA(DisplayName = "카드 전용"),
	CharacterAttack UMETA(DisplayName = "캐릭터 공격")
};

/** 카드 연출 이펙트를 어느 캐릭터 기준으로 붙일지 결정한다. */
UENUM(BlueprintType)
enum class EBGCardFXAnchor : uint8
{
	Instigator UMETA(DisplayName = "카드 사용자"),
	Target     UMETA(DisplayName = "카드 대상")
};

/**
 * 카드 규칙 DT 와 분리된 연출 전용 Row.
 *
 * Row 이름은 CardId 태그 문자열과 동일하게 둔다. 공격 카드는 CharacterAttack 정책만 갖고,
 * 실제 공격/피격 Niagara는 FBGCharacterRow::AttackFX에서 직업별로 가져온다.
 */
USTRUCT(BlueprintType)
struct FBGCardFXRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Card.Id"))
	FGameplayTag CardId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	EBGCardFXSource FXSource = EBGCardFXSource::CardSpecific;

	/** CardSpecific일 때 Primary(카드 사용자 연출) 진입 시 재생. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Primary")
	TSoftObjectPtr<UNiagaraSystem> PrimaryEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Primary")
	EBGCardFXAnchor PrimaryAnchor = EBGCardFXAnchor::Instigator;

	/** Primary(공격) 단계 진입 후 Niagara를 스폰하기까지 기다리는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float PrimarySpawnDelaySeconds = 0.1f;

	/** Secondary(피격/결과 연출) 진입 시 판정별 재생. 비우면 해당 결과에는 이펙트가 없다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secondary")
	TSoftObjectPtr<UNiagaraSystem> CriticalFailureEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secondary")
	TSoftObjectPtr<UNiagaraSystem> FailureEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secondary")
	TSoftObjectPtr<UNiagaraSystem> SuccessEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secondary")
	TSoftObjectPtr<UNiagaraSystem> CriticalSuccessEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secondary")
	EBGCardFXAnchor SecondaryAnchor = EBGCardFXAnchor::Target;

	/** Secondary(피격) 단계의 카메라 이동이 끝난 뒤 보이도록 기다리는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float SecondarySpawnDelaySeconds = 0.2f;

	/** 선택적 메쉬 소켓. 비우면 캐릭터 Root에 붙어 Actor의 +X 전방을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FName AttachSocket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform", meta = (ClampMin = "0.01"))
	float Scale = 1.f;

	/** 종료 연출 신호. 현재 Niagara 자산의 공통 이름은 User.bIsEnding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifetime")
	FName EndBoolParameter = TEXT("User.bIsEnding");

	/** 종료 bool 전달 뒤 시스템을 비활성화할 때까지 기다리는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifetime", meta = (ClampMin = "0.0"))
	float EndDelaySeconds = 1.f;

	/** 비시네마틱 카드가 Cue를 활성 상태로 유지하는 시간. 이 뒤 Remove가 종료 bool을 전달한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifetime", meta = (ClampMin = "0.01", Units = "s"))
	float NonCinematicCueSeconds = 1.f;
};

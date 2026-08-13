// BANG! 버프/디버프 이펙트 DT Row 타입.
//
// Card/BGGameDataTypes.h 에 같이 두지 않은 이유는 UI/View 를 가른 것과 같다 —
// 규칙 쪽(DataSubsystem·PlayerController·DiceComponent)이 읽는 Row 와 화면만 읽는 Row 를
// 한 헤더에 섞으면 규칙 코드가 나이아가라를 알게 된다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "BGStatusFXTypes.generated.h"

class UNiagaraSystem;

/**
 * 캐릭터에 계속 붙어 있는 이펙트 1종 (장비 오라 · 상태 표식).
 *
 * ⚠️ **Row 이름이 곧 조회 키다.** 출처가 둘인데 표는 하나다:
 *      장비 → FBGCardInstance::CardId        예) "Card.Id.FairyArmor"
 *      상태 → FBGPendingStatus::Tag 의 이름   예) "Status.NextAttack.DamageBonus"
 *    둘 다 FName 이고, DT_BGCard 의 Row 이름도 태그 문자열이라 그대로 맞아떨어진다.
 *    Row 이름을 태그 문자열과 다르게 지으면 **경고 없이 조용히 안 뜬다.**
 *
 * 여기 없는 키는 "이펙트 없음" 이다. 장비·상태를 추가할 때마다 이 표를 채워야 하는 것은
 * 아니며, 표에 없다고 규칙이 달라지지도 않는다.
 */
USTRUCT(BlueprintType)
struct FBGStatusFXRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * 확인용 사본. 조회는 Row 이름으로 하므로 이 값을 고쳐도 조회 결과는 바뀌지 않는다.
	 * (FBGCharacterRow::CharacterTag 와 같은 성격이다)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag SourceTag;

	/** 디버그·툴팁용 이름. 화면에 꼭 나와야 하는 값은 아니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	/**
	 * 붙일 나이아가라. 비워 두면 이 키는 이펙트가 없는 것으로 친다(경고하지 않는다) —
	 * 이펙트를 아직 안 만든 장비·상태도 Row 만 미리 올려 둘 수 있게 한 것이다.
	 *
	 * ⚠️ 소프트 참조다. 하드로 두면 표를 여는 순간 쓰지도 않을 이펙트가 전부 메모리에 올라온다
	 *    (DT_BGCard 의 ArtTexture 와 같은 이유).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UNiagaraSystem> Effect;

	/**
	 * 캐릭터 메쉬의 어느 소켓에 붙는가. 비우면 메쉬 원점(발밑)이다.
	 *
	 * ⚠️ 직업 8종이 메쉬를 갈아끼우므로(FBGCharacterRow::SkeletalMesh) **공용 스켈레톤에
	 *    있는 소켓만 쓸 것.** 없는 이름을 적으면 조용히 원점에 붙어 발밑에서 터진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AttachSocket;

	/** 소켓 기준 추가 오프셋. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float Scale = 1.f;
};

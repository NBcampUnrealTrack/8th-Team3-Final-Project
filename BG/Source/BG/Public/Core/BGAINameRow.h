// PvE 에서 AI 좌석에 붙일 이름 표.
//
// 한 행이 이름 하나다. Row 이름은 아무거나 상관없다(Name_1, Name_2 ...) - 화면에 나가는 건 DisplayName 이다.
// Role 은 이 이름을 쓸 역할이다. 판이 시작되어 AI 의 역할이 정해진 뒤, 그 역할 행들을
// 표에 적힌 순서대로 하나씩 붙인다(같은 판에서 겹치지 않는다).
// 해당 역할의 이름이 없거나 모자라면 "AI 1" 처럼 번호 이름이 그대로 남는다.
//
// PvE 에서 AI 가 맡는 역할은 Role.Deputy(부관)와 Role.Outlaw(무법자)다. 보안관은 사람이라 쓰이지 않는다.
// BP_BGGameMode 의 BG|PvE > AINameTable 에 이 구조체로 만든 DataTable 을 꽂는다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "BGAINameRow.generated.h"

USTRUCT(BlueprintType)
struct FBGAINameRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 이름을 쓸 역할(Role.Deputy / Role.Outlaw). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Role"))
	FGameplayTag Role;

	/** AI 좌석에 보일 이름. 비워 두면 그 행은 건너뛴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;
};

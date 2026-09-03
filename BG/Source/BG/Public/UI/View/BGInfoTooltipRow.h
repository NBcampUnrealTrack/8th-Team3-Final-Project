// BANG! 툴팁 문구 표 — 화면에 나가는 설명문을 코드 밖으로 뺀다.
//
// Row 이름이 곧 키다(DT_BGCharacterRow 와 같은 규약):
//   Info.Job / Info.Equipment / Info.HandLimit / Info.Role  — 각 줄이 무엇인지
//   Role.Sheriff / Role.Deputy / ...                        — Row 이름 = 태그 문자열
//
// 직업·장비 설명은 여기 없다. 종류마다 다른 값이라 게임 데이터 쪽
// (DT_BGCharacterRow / DT_BGCard 의 Description)이 갖는다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BGInfoTooltipRow.generated.h"

USTRUCT(BlueprintType)
struct FBGInfoTooltipRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 제목. 살아 있는 값(직업·역할 이름)이 있으면 그쪽이 이긴다 — 여기 것은 폴백이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	/** 본문. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText Body;
};

// PvE 런 결과 화면용 조우 한 건의 기록.
//
// 조우마다 맵이 새로 로드되므로 UBGGameInstance 가 쌓고, 런이 끝나는 순간
// ABGGameState 의 복제 배열로 옮겨 결과 화면이 읽는다.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BGPvERunRecord.generated.h"

USTRUCT(BlueprintType)
struct FBGPvEEncounterRecord
{
	GENERATED_BODY()

	/** 조우 번호(1-base). */
	UPROPERTY(BlueprintReadOnly, Category = "BG|PvE")
	int32 EncounterIndex = 0;

	/** 사람 플레이어가 그 조우에서 받은 직업. 조우마다 바뀐다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|PvE")
	FGameplayTag JobTag;

	/** 아군 수(보안관 + 부관). */
	UPROPERTY(BlueprintReadOnly, Category = "BG|PvE")
	int32 AllyCount = 0;

	/** 적 수(보안관 진영이 아닌 좌석). */
	UPROPERTY(BlueprintReadOnly, Category = "BG|PvE")
	int32 EnemyCount = 0;

	/** 이 조우를 이겼는가. false면 여기서 런이 끝났다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|PvE")
	bool bCleared = false;

	/** 조우를 플레이한 시간(초). */
	UPROPERTY(BlueprintReadOnly, Category = "BG|PvE")
	float Seconds = 0.f;
};

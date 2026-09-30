// BANG! 맵별 엔딩 시퀀스 목록 — 승리 진영마다 어떤 성 연출을 틀지 이 맵에서 정한다.
//
// ── 왜 맵에 두는가 ──────────────────────────────────────────────
//  시퀀스는 그 맵의 성을 찍은 것이라 맵에 속한 정보다. PlayerController 에 두면 맵이 늘 때마다
//  컨트롤러를 복제하거나 맵 이름으로 표를 뒤져야 한다 — 맵을 옮기거나 이름을 바꾸면 깨진다.
//
//  ⚠️ 시퀀스를 **하드 참조**로 든다. 레벨과 함께 로드되므로 연출이 시작될 때 기다릴 것이 없다.
//     성 붕괴처럼 Geometry Collection 이 딸린 무거운 시퀀스에서 이 차이가 크다.
//
//  복제하지 않는다. 레벨에 배치된 액터라 서버·클라 모두가 각자 자기 월드에서 찾는다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "BGEndingSequenceSet.generated.h"

class ULevelSequence;

UCLASS(Blueprintable)
class BG_API ABGEndingSequenceSet : public AActor
{
	GENERATED_BODY()

public:
	ABGEndingSequenceSet();

	/** 이 맵의 목록에서 승리 진영에 맞는 시퀀스를 찾는다. 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "BG|Ending")
	ULevelSequence* FindSequence(FGameplayTag WinningRole) const;

	/**
	 * 지금 월드에 배치된 목록. 없으면 nullptr —
	 * 그때는 PlayerController 에 적어 둔 기본 목록을 쓴다(맵별로 나눌 필요가 없는 경우).
	 *
	 * ⚠️ 맵에 둘 이상 배치하면 첫 번째만 쓰이고 경고를 남긴다.
	 */
	static ABGEndingSequenceSet* Find(const UObject* WorldContextObject);

protected:
	/**
	 * 승리 진영(Role.*) → 재생할 시퀀스.
	 *   Role.Sheriff  — 지켜낸 새벽
	 *   Role.Outlaw   — 무너진 왕국
	 *   Role.Renegade — 빼앗긴 왕좌
	 *
	 * 비워 둔 진영은 시퀀스 단계 동안(GameState 의 EndingSequenceSeconds) 빈 화면으로 기다린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|Ending")
	TMap<FGameplayTag, TObjectPtr<ULevelSequence>> Sequences;
};

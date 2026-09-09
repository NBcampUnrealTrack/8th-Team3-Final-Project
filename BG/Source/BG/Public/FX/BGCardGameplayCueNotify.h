#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "BGCardGameplayCueNotify.generated.h"

class UNiagaraComponent;

/**
 * 카드 시네마틱용 지속형 GameplayCue Notify 공통 구현.
 *
 * GC_Card_Primary / GC_Card_Secondary Blueprint가 이 클래스를 상속한다.
 * GameState는 대상 ASC에 Cue를 Add/Remove만 하며, 이 클래스가 카드/직업 DT를 읽어
 * Niagara 선택, 스폰, 종료 bool 전달, 지연 비활성화를 맡는다.
 */
UCLASS(Blueprintable)
class BG_API ABGCardGameplayCueNotify : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ABGCardGameplayCueNotify();

	virtual bool OnActive_Implementation(AActor* MyTarget,
		const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* MyTarget,
		const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget,
		const FGameplayCueParameters& Parameters) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 확장 지점: 카드/직업/판정에 맞는 Anim Montage 재생을 Blueprint에서 구현한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|GameplayCue", meta = (DisplayName = "Play Card Animation"))
	void K2_PlayCardAnimation(AActor* Target, const FGameplayCueParameters& Parameters);

	/** 확장 지점: 카드/직업/판정에 맞는 Sound 시작을 Blueprint에서 구현한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|GameplayCue", meta = (DisplayName = "Play Card Sound"))
	void K2_PlayCardSound(AActor* Target, const FGameplayCueParameters& Parameters);

	/** 확장 지점: Remove 시 Montage/Sound 정지 또는 페이드아웃을 Blueprint에서 구현한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|GameplayCue", meta = (DisplayName = "Stop Card Animation And Sound"))
	void K2_StopCardAnimationAndSound(AActor* Target, const FGameplayCueParameters& Parameters);

private:
	void SpawnCardNiagara(AActor* MyTarget, const FGameplayCueParameters& Parameters,
		bool bAllowSpawnDelay = true);
	void FinishCueRemoval();

	/**
	 * 로컬 PlayerController → 손패 순으로 찾아 "카드 연출 끝" 을 알린다.
	 * ⚠️ 시네마틱 카드는 여기로 오지 않는다 — 단계마다 Cue 가 떨어져 중간에 꺼진다.
	 *    그쪽 끝은 PlayerController 의 연출 단계 통지(Stage == None)가 맡는다.
	 */
	void NotifyHandPlayFXFinished();

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SpawnedEffect;

	FName EndBoolParameter = NAME_None;
	float EndDelaySeconds = 0.f;
	FTimerHandle SpawnTimer;
	FTimerHandle EndTimer;
};

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "UI/Presentation/BGPresentationTypes.h"
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

	/** 사용자 연출 배속 배율. GameState 가 없으면 1. 스폰/종료 지연을 단계 길이와 같은 비율로 줄인다. */
	float GetUserSpeedScale() const;

	// ── 월드FX 트랙 ────────────────────────────────────────────────
	//	이 Cue 가 사는 동안 표를 쥔다. 시네마틱 여부는 따지지 않는다 —
	//	시네마틱이 도는 동안은 그쪽 트랙이 따로 표를 쥐고 있다.

	void BeginWorldFXTrack();
	void EndWorldFXTrack();

	/** Cue 가 사는 동안 쥐고 있는 표. */
	FBGPresentationTrackHold WorldFXHold;

	// ── 몽타주 종료를 큐의 표로 바꾼다 ─────────────────────────────
	//	BP 가 무슨 몽타주를 걸었는지 몰라도, 재생 직후 그 메쉬에서 "돌고 있는 몽타주" 를
	//	잡아 표를 쥔다 — 그래서 카드마다 BP 에 종료 핀을 배선하지 않아도 된다.
	//	⚠️ AnimInstance 의 멀티캐스트에 붙는다. Montage_SetEndDelegate 는 BP 의
	//	   Play Montage 노드가 쓰는 자리라, 쓰면 BP 의 On Completed 가 죽는다.

	/** 몽타주 재생 **직후**에 호출할 것. 한 프레임만 늦어도 못 잡는다. */
	void WatchCardAnimation(AActor* MyTarget);

	UFUNCTION()
	void HandleCardMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** 구독을 풀고 쥔 표를 놓는다. 여러 번 불러도 안전하다. */
	void StopWatchingCardAnimation();

	TWeakObjectPtr<class UAnimInstance> WatchedAnimInstance;
	TWeakObjectPtr<class UAnimMontage>  WatchedMontage;

	/** 몽타주가 끝날 때까지 쥐고 있는 표. */
	FBGPresentationTrackHold MontageHold;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SpawnedEffect;

	FName EndBoolParameter = NAME_None;
	float EndDelaySeconds = 0.f;
	FTimerHandle SpawnTimer;
	FTimerHandle EndTimer;
};

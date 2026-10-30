// BANG! 카드/공격 연출 FX 노티파이 — 공격·카드 몽타주에 심어 두면 그 프레임에서
// ABGCharacter::NotifyCardFXAnimCue() 를 불러 카드 Niagara 를 지연 없이 즉시 스폰한다.
//
// ⚠️ 규칙에 관여하지 않는다. 순수 연출 타이밍이다. 몽타주에 이 노티파이를 안 심어도
//    문제없다 — ABGCardGameplayCueNotify 가 몽타주를 스캔해 이 노티파이가 없으면
//    DT 의 PrimarySpawnDelaySeconds/SecondarySpawnDelaySeconds 딜레이로 대신 스폰한다.
//    즉 이 노티파이는 "있으면 그 프레임이 우선, 없으면 기존 딜레이 스폰으로 자동 대체" 다.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "BGAnimNotify_CardFX.generated.h"

UCLASS(meta = (DisplayName = "BG 카드 FX 스폰"))
class BG_API UBGAnimNotify_CardFX : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};

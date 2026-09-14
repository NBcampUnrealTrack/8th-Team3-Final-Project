// BANG! 정체 공개 스왑 노티파이 — 쓰러짐/역할공개 몽타주에 심어 두면 그 프레임에서
// ABGCharacter::RefreshRoleRevealVisual() 을 불러 본체 메시를 정체 공개 메시로 바꾼다.
//
// ⚠️ 규칙에 관여하지 않는다. 순수 연출 타이밍이라 노티파이가 빠지거나 밀려도 판정은 그대로
//    돌아간다 — Dead 상태 진입 시의 안전망이 결국은 맞춰 준다(ABGCharacter.h 주석 참고).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "BGAnimNotify_RoleReveal.generated.h"

UCLASS(meta = (DisplayName = "BG 정체 공개 스왑"))
class BG_API UBGAnimNotify_RoleReveal : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};

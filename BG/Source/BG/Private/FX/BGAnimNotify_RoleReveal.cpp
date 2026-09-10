#include "FX/BGAnimNotify_RoleReveal.h"

#include "Components/SkeletalMeshComponent.h"
#include "Core/BGCharacter.h"

FString UBGAnimNotify_RoleReveal::GetNotifyName_Implementation() const
{
	return TEXT("BG 정체 공개 스왑");
}

void UBGAnimNotify_RoleReveal::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (ABGCharacter* Character = MeshComp ? Cast<ABGCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		Character->RefreshRoleRevealVisual();
	}
}

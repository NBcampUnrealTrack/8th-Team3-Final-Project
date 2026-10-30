#include "FX/BGAnimNotify_CardFX.h"

#include "Components/SkeletalMeshComponent.h"
#include "Core/BGCharacter.h"

FString UBGAnimNotify_CardFX::GetNotifyName_Implementation() const
{
	return TEXT("BG 카드 FX 스폰");
}

void UBGAnimNotify_CardFX::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (ABGCharacter* Character = MeshComp ? Cast<ABGCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		Character->NotifyCardFXAnimCue();
	}
}

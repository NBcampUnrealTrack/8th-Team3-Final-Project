#include "UI/Target/BGTargetHighlight.h"

#include "Components/SkeletalMeshComponent.h"
#include "Core/BGCharacter.h"

void BGTargetHighlight::Apply(ABGCharacter* Character, EState State)
{
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	const bool bOn = (State != EState::None);

	//	값을 먼저 넣고 켠다. 순서가 반대면 한 프레임 동안 이전 색으로 그려진다.
	Mesh->SetCustomDepthStencilValue(static_cast<int32>(State));
	Mesh->SetRenderCustomDepth(bOn);
}

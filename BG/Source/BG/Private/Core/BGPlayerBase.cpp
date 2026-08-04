#include "Core/BGPlayerBase.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

ABGPlayerBase::ABGPlayerBase()
{
	//	구조만 잡은 단계라 틱이 필요 없다. 필요해지면 켤 것 —
	//	폰이 늘어난 뒤에 켜면 인원수만큼 빈 틱이 돈다.
	PrimaryActorTick.bCanEverTick = false;

	//	캡슐을 루트로 쓰지 않으므로 빈 씬 컴포넌트를 루트로 만든다.
	//	RootComponent 를 지정하지 않으면 엔진이 임의의 컴포넌트를 루트로 삼아
	//	계층이 의도와 달라진다.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->SetupAttachment(Root);
	//	ACharacter 의 기본값과 맞춘다 — 나중에 ACharacter 로 갈아타도 크기가 달라지지 않는다.
	Capsule->InitCapsuleSize(34.f, 88.f);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);
	//	메시는 보이기만 하면 된다. 충돌은 캡슐이 전담한다 —
	//	둘 다 충돌을 켜면 같은 액터가 두 번 맞아 피해가 두 배로 들어간다.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//	두 카메라 모두 루트에 붙는다(캡슐이 아니다). 캡슐의 회전을 따라가지 않는다.
	PrimaryCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PrimaryCamera"));
	PrimaryCamera->SetupAttachment(Root);

	SecondaryCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SecondaryCamera"));
	SecondaryCamera->SetupAttachment(Root);

	//	활성 카메라는 항상 하나여야 한다 — 이유는 SetActiveCamera 주석 참고.
	PrimaryCamera->SetAutoActivate(true);
	SecondaryCamera->SetAutoActivate(false);
}

void ABGPlayerBase::SetActiveCamera(bool bUsePrimary)
{
	if (!PrimaryCamera || !SecondaryCamera)
	{
		return;
	}

	//	끄는 쪽을 먼저 처리한다. 켜는 쪽을 먼저 하면 두 카메라가 동시에 활성인 순간이 생기고,
	//	그 프레임에 CalcCamera 가 돌면 엉뚱한 시점이 한 프레임 섞여 보인다.
	UCameraComponent* Next     = bUsePrimary ? PrimaryCamera : SecondaryCamera;
	UCameraComponent* Previous = bUsePrimary ? SecondaryCamera : PrimaryCamera;

	Previous->Deactivate();
	Next->Activate();
}

bool ABGPlayerBase::IsPrimaryCameraActive() const
{
	return PrimaryCamera && PrimaryCamera->IsActive();
}

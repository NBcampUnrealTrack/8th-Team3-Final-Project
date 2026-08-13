// TRMassEnemyActor.cpp

#include "Mass/Representation/TRMassEnemyActor.h"

#include "Components/SkeletalMeshComponent.h"

ATRMassEnemyActor::ATRMassEnemyActor()
{
	// 액터는 아무것도 계산하지 않는다. Mass가 값을 밀어넣으므로 틱이 필요 없다.
	// 개체가 수백이 되면 액터 틱 자체가 프레임을 먹는다.
	PrimaryActorTick.bCanEverTick = false;

	// 루트를 빈 씬 컴포넌트로 두고 메시를 자식으로 붙인다.
	// 메시가 루트면 상대 트랜스폼을 편집할 수 없어 정면 보정을 못 한다.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(SceneRoot);

	// 표현용 액터라 충돌 판정은 필요 없다. 원거리 개체는 액터 자체가 없으므로
	// 적중 판정을 액터 콜리전에 의존하면 안 되고, Mass 공간 질의로 처리해야 한다.
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetGenerateOverlapEvents(false);
	MeshComp->bReceivesDecals = false;

	// ── 군중용 애니메이션 비용 절감 ──────────────────────────────────────────
	// 화면에 안 보이면 포즈 계산을 건너뛴다. 카메라 뒤의 개체가 공짜가 된다.
	MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	// 거리에 따라 애니메이션 평가 빈도를 낮추고 사이를 보간한다.
	// 멀리 있는 개체는 매 프레임 평가하지 않아도 티가 나지 않는다.
	MeshComp->bEnableUpdateRateOptimizations = true;

	// 100개가 매 프레임 내비메시를 더럽히지 않도록 한다.
	SetCanBeDamaged(false);
	MeshComp->SetCanEverAffectNavigation(false);

	// 높이 오프셋은 0이다. ACharacter는 캡슐 중심이 기준이라 메시를 -88 내리지만,
	// Mass 엔티티의 트랜스폼은 지면 스냅 결과, 즉 발밑이 기준이다. 여기서 내리면 묻힌다.
	//
	// Yaw만 보정한다. 언리얼 캐릭터 메시는 -Y를 정면으로 만들어져 있어서
	// -90도 돌려야 액터의 +X(진행 방향)와 정면이 맞는다.
	MeshComp->SetRelativeLocation(FVector::ZeroVector);
	MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// Mass가 위치를 직접 밀어넣으므로 액터 복제는 하지 않는다.
	bReplicates = false;
}

void ATRMassEnemyActor::SetMassState(const FVector& InVelocity, ETREnemyActionState InActionState,
	float InStateElapsed, float DeltaSeconds)
{
	// 속도는 보간한다. Mass 쪽 값이 프레임마다 조금씩 튀어도 애니메이션이 떨지 않게.
	CurrentVelocity = (SpeedInterpRate > 0.f && DeltaSeconds > 0.f)
		? FMath::VInterpTo(CurrentVelocity, InVelocity, DeltaSeconds, SpeedInterpRate)
		: InVelocity;

	// 지면이 스크롤하므로 지면 대비 속력을 따로 계산한다. 이 값으로 재생 속도를 맞춰야
	// 발이 미끄러지지 않는다.
	AnimData.GroundSpeed = (CurrentVelocity - GroundVelocity).Size();
	AnimData.LocalVelocity = GetActorRotation().UnrotateVector(CurrentVelocity);
	AnimData.ActionState = InActionState;
	AnimData.StateElapsed = InStateElapsed;
}

void ATRMassEnemyActor::PrepareForPooling_Implementation()
{
	SetActorHiddenInGame(true);

	CurrentVelocity = FVector::ZeroVector;
	AnimData = FTREnemyAnimData();
}

void ATRMassEnemyActor::PrepareForGame_Implementation()
{
	SetActorHiddenInGame(false);

	// 재사용된 액터가 이전 개체의 속도를 물고 시작하지 않도록 초기화한다.
	CurrentVelocity = FVector::ZeroVector;
	AnimData = FTREnemyAnimData();
}

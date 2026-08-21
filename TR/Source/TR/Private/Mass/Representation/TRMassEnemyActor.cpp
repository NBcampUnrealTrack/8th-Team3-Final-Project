// TRMassEnemyActor.cpp

#include "Mass/Representation/TRMassEnemyActor.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

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

void ATRMassEnemyActor::SetMassState(const UE::TR::Enemy::FAnimContext& Context,
	const UE::TR::Enemy::FAnimResolution& Anim, float DeltaSeconds)
{
	// 스냅샷으로 받은 사망 상태의 진입 순간에만 재생한다. 이 액터는 복제되지 않으므로 각 클라이언트가 로컬에서 판단한다.
	if (GetNetMode() != NM_DedicatedServer
		&& !bFirstUpdate
		&& PreviousActionState != ETREnemyActionState::Death
		&& Context.ActionState == ETREnemyActionState::Death
		&& IsValid(DeathEffect))
	{
		const bool bAutoDestroy = true;
		const bool bAutoActivate = true;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			DeathEffect,
			GetActorLocation() + FVector::UpVector * DeathEffectZOffset,
			GetActorRotation(),
			FVector::OneVector,
			bAutoDestroy,
			bAutoActivate,
			ENCPoolMethod::AutoRelease);
	}
	PreviousActionState = Context.ActionState;

	// 속도는 보간한다. Mass 쪽 값이 프레임마다 튀어도 애니메이션이 떨지 않게.
	// 갓 나타난 프레임만 예외다 — 0에서 기어오르면 방금 나타난 적이 잠깐 대기 모션을 재생한다.
	CurrentVelocity = (SpeedInterpRate > 0.f && DeltaSeconds > 0.f && !bFirstUpdate)
		? FMath::VInterpTo(CurrentVelocity, Context.Velocity, DeltaSeconds, SpeedInterpRate)
		: Context.Velocity;

	// 첫 스냅샷은 속도 보간과 사망 진입 연출에서 제외한다. 풀에서 이미 사망한 엔티티를 받아도 폭발을 반복하지 않는다.
	// 이 플래그는 액터가 스스로 내려 AnimBP 구성과 무관하게 한 프레임만 유지한다.
	bFirstUpdate = false;

	// 지면이 스크롤하므로 지면 대비 속도로 바꾼다. 이 값으로 재생 속도를 맞춰야 발이 미끄러지지 않는다.
	const FVector RelativeVelocity = CurrentVelocity - Context.GroundVelocity;

	// 수평 성분만 잰다. 발이 딛는 속력은 수평이고, 무엇보다 이동/대기를 가르는
	// UE::TR::Enemy::ResolveAnim도 Size2D를 쓴다. 여기만 3D면 낙하나 경사 구간에서
	// "대기 판정인데 재생 속도는 빠른" 상태가 된다.
	AnimData.GroundSpeed = RelativeVelocity.Size2D();
	AnimData.LocalVelocity = GetActorRotation().UnrotateVector(RelativeVelocity);
	AnimData.ActionState = Context.ActionState;
	AnimData.StateElapsed = Context.StateElapsed;
	AnimData.bPlayingOneShot = !Anim.bLooping;
	AnimData.bIsMoving = Anim.bMoving;

	// 로코모션은 매 프레임 갱신한다. 단발 중에도 값이 흘러야 블렌드가 매끄럽다.
	AnimData.LocomotionTime = Anim.LocomotionPosition;

	// 단발은 대응하는 클립이 있을 때만 덮는다. 상태가 None으로 돌아간 뒤에도 0.15초쯤
	// 블렌드 아웃이 남아 있는데, 여기서 0을 쓰면 물러나던 클립이 첫 프레임으로 되감긴다.
	if (Anim.bHasAction)
	{
		AnimData.ActionTime = Anim.ActionPosition;
	}
}

void ATRMassEnemyActor::UpdateDamageVisuals(float InDamageRatio, float DeltaSeconds)
{
	const float Clamped = FMath::Clamp(InDamageRatio, 0.f, 1.f);

	// ── 피격 플래시 ──────────────────────────────────────────────────────────
	//
	// 피격 이벤트를 따로 받지 않고 손상도가 **올라간 것**으로 감지한다. 전투 쪽에
	// 이벤트를 추가하면 그 이벤트가 표현 계층까지 오는 경로(프래그먼트든 델리게이트든)를
	// 하나 더 유지해야 하는데, 이미 매 틱 들어오는 값으로 같은 판단을 할 수 있다.
	//
	// 첫 갱신(LastDamageRatio < 0)에서는 터뜨리지 않는다. ISM 으로 있는 동안 맞은
	// 개체가 근거리로 넘어오는 순간 값이 0에서 껑충 뛰는데, 그건 방금 맞은 게 아니다.
	const bool bFirstDamageUpdate = (LastDamageRatio < 0.f);
	const bool bTookDamage = !bFirstDamageUpdate && (Clamped > LastDamageRatio + DamageWriteThreshold);

	if (bTookDamage && HitFlashDuration > 0.f)
	{
		HitFlash = 1.f;
	}
	else if (HitFlash > 0.f)
	{
		// 지속 시간이 0이면 즉시 꺼진다. 나눗셈을 피하려고 분기해 둔다.
		HitFlash = (HitFlashDuration > 0.f)
			? FMath::Max(0.f, HitFlash - DeltaSeconds / HitFlashDuration)
			: 0.f;
	}

	// 감쇠 중에는 매 틱 써야 하지만, 0에 닿은 뒤로는 쓸 것이 없다.
	if (!FMath::IsNearlyEqual(HitFlash, LastWrittenHitFlash, DamageWriteThreshold) || (HitFlash <= 0.f && LastWrittenHitFlash > 0.f))
	{
		LastWrittenHitFlash = HitFlash;
		MeshComp->SetCustomPrimitiveDataFloat(HitFlashCustomDataIndex, HitFlash);
	}

	// ── 손상도 ───────────────────────────────────────────────────────────────
	//
	// 체력은 피격 때만 변하는데 이 함수는 매 틱 불린다. 문턱이 없으면 개체 수백이
	// 매 프레임 렌더 상태를 더럽힌다.
	if (FMath::Abs(Clamped - LastDamageRatio) < DamageWriteThreshold)
	{
		return;
	}

	LastDamageRatio = Clamped;
	MeshComp->SetCustomPrimitiveDataFloat(DamageCustomDataIndex, Clamped);
}

void ATRMassEnemyActor::PrepareForPooling_Implementation()
{
	SetActorHiddenInGame(true);

	CurrentVelocity = FVector::ZeroVector;
	AnimData = FTREnemyAnimData();
	PreviousActionState = ETREnemyActionState::None;
}

void ATRMassEnemyActor::PrepareForGame_Implementation()
{
	SetActorHiddenInGame(false);

	// 재사용된 액터가 이전 개체의 속도를 물고 시작하지 않도록 초기화한다.
	CurrentVelocity = FVector::ZeroVector;
	AnimData = FTREnemyAnimData();
	PreviousActionState = ETREnemyActionState::None;

	// 값만 지워서는 부족하다. AnimBP의 상태머신은 이전 개체가 남긴 스테이트에 그대로 머물러 있다.
	bFirstUpdate = true;
	bPoseResetPending = true;

	// 상처도 물려받는다. 머테리얼에 즉시 0을 써서 멀쩡한 상태로 나가게 하고,
	// 마지막 값을 무효로 돌려 다음 갱신이 문턱에 걸려 생략되지 않게 한다.
	// 음수는 "아직 쓴 적 없음"이라 첫 갱신에서 피격 플래시가 잘못 터지는 것도 막는다.
	LastDamageRatio = -1.f;
	MeshComp->SetCustomPrimitiveDataFloat(DamageCustomDataIndex, 0.f);

	HitFlash = 0.f;
	LastWrittenHitFlash = 0.f;
	MeshComp->SetCustomPrimitiveDataFloat(HitFlashCustomDataIndex, 0.f);

	if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
	{
		// 스프링/시뮬레이션 노드가 이전 위치에서 튕겨나가는 것도 막는다.
		AnimInstance->ResetDynamics(ETeleportType::TeleportPhysics);
	}
}

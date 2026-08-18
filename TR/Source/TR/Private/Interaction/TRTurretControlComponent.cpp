// TRTurretControlComponent.cpp


#include "Interaction/TRTurretControlComponent.h"
#include "Debug/TRGameplayLog.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TRActionComponent.h"
#include "Interaction/TRInteractionComponent.h"
#include "Camera/CameraComponent.h"
#include "Core/TRPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TRPlayerCharacter.h"
#include "Train/TRInstallablePart.h"
#include "Train/TRTrain.h"
#include "Train/TRTurret.h"

// 아군 구조물 — 기차 선체와 설치 파츠(장갑·포탑).
static bool IsFriendlyStructure(const AActor* Actor)
{
	return IsValid(Actor) && (Actor->IsA<ATRTrain>() || Actor->IsA<ATRInstallablePart>());
}

UTRTurretControlComponent::UTRTurretControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTRTurretControlComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTRTurretControlComponent, MountedTurret);
}

bool UTRTurretControlComponent::ServerMount_Validate(ATRTurret* Turret)
{
	return Turret != nullptr;
}

void UTRTurretControlComponent::ServerMount_Implementation(ATRTurret* Turret)
{
	if (!IsValid(Turret) || IsMounted() || !Turret->CanBeMounted())
	{
		UE_LOG(LogTRGameplay, Verbose, TEXT("Mount ignored: invalid/occupied/destroyed"));
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn))
	{
		return;
	}

	// 클라이언트가 임의의 터렛 레퍼런스를 넘길 수 있으므로, 실제 상호작용 대상과 일치하는지 서버에서 재검증
	const UTRInteractionComponent* Interaction = OwnerPawn->FindComponentByClass<UTRInteractionComponent>();
	if (!IsValid(Interaction) || Interaction->GetCurrentTarget() != Turret)
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("Mount rejected: %s is not %s's current interaction target"), *Turret->GetName(), *OwnerPawn->GetName());
		return;
	}

	if (UTRActionComponent* Action = OwnerPawn->FindComponentByClass<UTRActionComponent>())
	{
		if (Action->IsActionInProgress())
		{
			Action->ServerCancelAction();
		}
	}

	PendingMountTurret = Turret;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(MountTimerHandle, this, &UTRTurretControlComponent::CompleteMount, MountDelay, false);
	}
}

void UTRTurretControlComponent::CompleteMount()
{
	ATRTurret* Turret = PendingMountTurret.Get();
	PendingMountTurret = nullptr;

	if (!IsValid(Turret) || !Turret->CanBeMounted())
	{
		UE_LOG(LogTRGameplay, Verbose, TEXT("Mount failed after delay: target no longer valid"));
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn))
	{
		return;
	}

	Turret->SetOccupant(OwnerPawn);
	MountedTurret = Turret;
	OnRep_MountedTurret();
	UE_LOG(LogTRGameplay, Verbose, TEXT("Mounted: %s"), *Turret->GetName());
}

void UTRTurretControlComponent::ServerDismount_Implementation()
{
	ForceDismount();
}

void UTRTurretControlComponent::ForceDismount()
{
	// public 함수라 외부 액터(파괴된 포탑 등)에서도 불린다. EndPlay 경로에서는 오너가 이미 정리 중일 수 있다.
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MountTimerHandle);
	}
	PendingMountTurret = nullptr;

	if (IsValid(MountedTurret))
	{
		UE_LOG(LogTRGameplay, Verbose, TEXT("Dismounted: %s"), *MountedTurret->GetName());
		MountedTurret->SetOccupant(nullptr);
	}
	MountedTurret = nullptr;
	OnRep_MountedTurret();
}

void UTRTurretControlComponent::ServerFire_Implementation()
{
	if (!IsValid(MountedTurret))
	{
		return;
	}

	if (MountedTurret->GetCurrentAmmo() <= 0)
	{
		if (!TryAutoRefillFromInventory())
		{
			UE_LOG(LogTRGameplay, Verbose, TEXT("Fire blocked: out of ammo"));
			return;
		}
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* Controller = IsValid(OwnerPawn) ? OwnerPawn->GetController() : nullptr;
	if (!IsValid(Controller))
	{
		return;
	}

	const FRotator CamRot = Controller->GetControlRotation();
	if (!MountedTurret->IsWithinFireArc(CamRot.Yaw, CamRot.Pitch))
	{
		UE_LOG(LogTRGameplay, Verbose, TEXT("Fire blocked: out of arc"));
		return;
	}

	const float FireInterval = 1.f / FMath::Max(MountedTurret->GetManualFireRate(), 0.01f);   // 0으로 나누기 방지
	const float Now = GetWorld()->GetTimeSeconds();
	if (LastFireTime >= 0.f && Now - LastFireTime < FireInterval)
	{
		return;
	}
	LastFireTime = Now;

	MountedTurret->ConsumeAmmo(1);

	FHitResult Hit;
	if (!TraceFireTarget(Hit))
	{
		UE_LOG(LogTRGameplay, Verbose, TEXT("Fire missed"));
		return;
	}

	AActor* HitActor = Hit.GetActor();
	if (!IsValid(HitActor))
	{
		return;
	}

	if (IsFriendlyStructure(HitActor))
	{
		UE_LOG(LogTRGameplay, Verbose, TEXT("Fire ignored: friendly structure %s"), *HitActor->GetName());
		return;
	}

	UGameplayStatics::ApplyDamage(HitActor, MountedTurret->GetManualDamage(), Controller, MountedTurret, nullptr);
	UE_LOG(LogTRGameplay, Verbose, TEXT("Fire hit: %s"), *HitActor->GetName());
}

bool UTRTurretControlComponent::TraceFireTarget(FHitResult& OutHit) const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* Controller = IsValid(OwnerPawn) ? OwnerPawn->GetController() : nullptr;
	if (!IsValid(Controller) || !IsValid(MountedTurret))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MountedTurret->GetManualRange();

	// 카메라가 포탑 메시에 붙어 있어 시작점이 포탑 안이다. 무시하지 않으면 자기가 탄 포탑을 맞춘다.
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerPawn);
	QueryParams.AddIgnoredActor(MountedTurret);

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const bool bHit = World->LineTraceSingleByChannel(OutHit, ViewLocation, TraceEnd, ECC_Pawn, QueryParams);

#if !UE_BUILD_SHIPPING
	// 초록=명중, 빨강=빗나감. 사격 판정이 서버에서 도니 서버 화면에만 그려진다.
	DrawDebugLine(World, ViewLocation, bHit ? OutHit.ImpactPoint : TraceEnd, bHit ? FColor::Green : FColor::Red, false, 2.f, 0, 1.5f);
#endif

	return bHit;
}

bool UTRTurretControlComponent::TryAutoRefillFromInventory()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	ATRPlayerState* PS = IsValid(OwnerPawn) ? Cast<ATRPlayerState>(OwnerPawn->GetPlayerState()) : nullptr;
	if (!IsValid(PS) || !IsValid(MountedTurret))
	{
		return false;
	}

	if (PS->GetCarriedSupply() <= 0)
	{
		return false;
	}

	PS->ConsumeCarriedSupply(1);
	MountedTurret->AddAmmo(MountedTurret->GetManualAmmoPerResource());
	UE_LOG(LogTRGameplay, Verbose, TEXT("Auto-refilled ammo: +%d"), MountedTurret->GetManualAmmoPerResource());
	return true;
}

void UTRTurretControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ForceDismount();
	Super::EndPlay(EndPlayReason);
}

APlayerCameraManager* UTRTurretControlComponent::GetLocalCameraManager() const
{
	const APawn* OwnerPawn = GetOwner<APawn>();
	const APlayerController* PC = IsValid(OwnerPawn) ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	return IsValid(PC) ? PC->PlayerCameraManager : nullptr;
}

// 탑승 중에는 사각 밖을 아예 못 보게 시야를 묶는다.
// 서버의 IsWithinFireArc 검사는 그대로 남는다 — 이건 피드백용이고,
// 조작된 회전값을 거르는 건 여전히 서버 몫이다.
void UTRTurretControlComponent::ApplyMountedViewLimits(const ATRTurret* Turret)
{
	APlayerCameraManager* CameraManager = GetLocalCameraManager();
	if (!IsValid(CameraManager) || !IsValid(Turret))
	{
		return;
	}

	// 이미 저장돼 있으면 덮어쓰지 않는다 — 포탑을 갈아탈 때 원본이 아니라
	// 직전 포탑의 제한값을 저장해 버리면 하차 후에도 시야가 묶인 채로 남는다.
	if (!bViewLimitsSaved)
	{
		SavedViewYawMin = CameraManager->ViewYawMin;
		SavedViewYawMax = CameraManager->ViewYawMax;
		SavedViewPitchMin = CameraManager->ViewPitchMin;
		SavedViewPitchMax = CameraManager->ViewPitchMax;
		bViewLimitsSaved = true;
	}

	const float HalfAngle = Turret->GetFireArcHalfAngle();

	// 반각이 180 이상이면 전방위라 Yaw 는 묶지 않는다.
	// FMath::ClampAngle 은 폭 360 을 0 으로 접어버려서, 그대로 넣으면 시야가 한 점에 고정된다.
	if (HalfAngle < 180.f)
	{
		CameraManager->ViewYawMin = Turret->GetInstallYaw() - HalfAngle;
		CameraManager->ViewYawMax = Turret->GetInstallYaw() + HalfAngle;
	}

	// Pitch 는 위아래로 90도를 넘길 수 없다. 사각이 그보다 넓어도 여기서 잘린다.
	const float PitchHalfAngle = FMath::Min(HalfAngle, 89.9f);
	CameraManager->ViewPitchMin = Turret->GetInstallPitch() - PitchHalfAngle;
	CameraManager->ViewPitchMax = Turret->GetInstallPitch() + PitchHalfAngle;
}

void UTRTurretControlComponent::ClearMountedViewLimits()
{
	APlayerCameraManager* CameraManager = GetLocalCameraManager();
	if (!IsValid(CameraManager) || !bViewLimitsSaved)
	{
		return;
	}

	CameraManager->ViewYawMin = SavedViewYawMin;
	CameraManager->ViewYawMax = SavedViewYawMax;
	CameraManager->ViewPitchMin = SavedViewPitchMin;
	CameraManager->ViewPitchMax = SavedViewPitchMax;
	bViewLimitsSaved = false;
}

void UTRTurretControlComponent::OnRep_MountedTurret()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		return;   // 카메라는 이 값을 실제로 "보는" 로컬 플레이어만 처리
	}

	// 시야 제한은 카메라 부착과 별개로 처리한다 —
	// 카메라 컴포넌트를 못 찾아 아래에서 리턴하더라도 제한은 반드시 원복돼야 한다.
	if (IsValid(MountedTurret))
	{
		ApplyMountedViewLimits(MountedTurret);
	}
	else
	{
		ClearMountedViewLimits();
	}

	ATRPlayerCharacter* Character = Cast<ATRPlayerCharacter>(OwnerPawn);
	UCameraComponent* Camera = IsValid(Character) ? Character->GetCameraComponent() : nullptr;
	if (!IsValid(Camera))
	{
		return;
	}

	if (IsValid(MountedTurret))
	{
		// user
		// 포탑 이동 전 캐릭터 카메라 위치 저장
		if (!bCameraLastTransform)
		{
			LastCameraTransform = Camera->GetRelativeTransform();
			bCameraLastTransform = true;
		}
		
		// 터렛 시점으로 전환
		Camera->AttachToComponent(MountedTurret->GetTurretMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale /* , 소켓명 */);
	}
	else
	{
		// 원래 자리(캐릭터 머리)로 복귀 — 생성자에서 붙인 위치와 동일
		Camera->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("head"));
		
		// user
		// 포탑 직전 카메라 위치 복원
		if (bCameraLastTransform)
		{
			Camera->SetRelativeTransform(LastCameraTransform);
			bCameraLastTransform = false;
		}
	}
}

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
#include "Mass/Combat/TRMassCombatSubsystem.h"

// 아군 구조물: 기차 선체와 설치 파츠(장갑·포탑).
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

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	
	UTRMassCombatSubsystem* Combat = World->GetSubsystem<UTRMassCombatSubsystem>();
	if (!IsValid(Combat))
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("Fire aborted: TRMassCombatSubsystem not found"));
		return;
	}

	// 발사 원점은 카메라 컴포넌트의 위치다.
	ATRPlayerCharacter* Character = Cast<ATRPlayerCharacter>(OwnerPawn);
	UCameraComponent* Camera = IsValid(Character) ? Character->GetCameraComponent() : nullptr;
	if (!IsValid(Camera))
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("Fire aborted: camera component not found"));
		return;
	}
	
	const FRotator CamRot = Controller->GetControlRotation();
	if (!MountedTurret->IsWithinFireArc(CamRot.Yaw, CamRot.Pitch))
	{
		UE_LOG(LogTRGameplay, Verbose, TEXT("Fire blocked: out of arc"));
		return;
	}

	const float FireInterval = 1.f / FMath::Max(MountedTurret->GetManualFireRate(), 0.01f);   // 0으로 나누기 방지
	const float Now = World->GetTimeSeconds();
	if (LastFireTime >= 0.f && Now - LastFireTime < FireInterval)
	{
		return;
	}
	LastFireTime = Now;

	MountedTurret->ConsumeAmmo(1);

	const FVector FireOrigin = Camera->GetComponentLocation();
	const FVector FireDirection = CamRot.Vector();

	// 액터 쪽을 먼저 재서 유효 사거리를 정한다. 무언가에 막혔으면 그 지점까지만 적을 찾는다.
	FHitResult Hit;
	const bool bActorHit = TraceFireTarget(FireOrigin, FireDirection, Hit);
	const float EnemyRange = bActorHit ? Hit.Distance : MountedTurret->GetManualRange();

	// Mass 적 판정. FireHitscan 안에서 체력 차감·사망 처리·OnEnemyKilled 까지 끝낸다.
	const FTRMassHitResult EnemyHit = Combat->FireHitscan(
		FireOrigin, FireDirection, EnemyRange, MountedTurret->GetManualDamage());

	// 연출은 포탑이 전원에게 알린다. 적을 맞혔으면 그 지점, 액터에 막혔으면 막힌 지점, 둘 다 아니면 사거리 끝.
	const FVector EffectLocation = EnemyHit.bHit ? EnemyHit.Location : (bActorHit ? Hit.ImpactPoint : FireOrigin + FireDirection * EnemyRange);
	MountedTurret->MulticastFireEffect(EffectLocation, EnemyHit.bHit);

	if (EnemyHit.bHit)
	{
		UE_LOG(LogTRGameplay, Verbose, TEXT("Fire hit enemy(%s): remaining %.0f%s"),
			LexToString(EnemyHit.EnemyType), EnemyHit.RemainingHealth, EnemyHit.bKilled ? TEXT(" [killed]") : TEXT(""));
		return;
	}

	// 적이 없으면 액터 판정 결과를 쓴다.
	if (!bActorHit)
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

bool UTRTurretControlComponent::TraceFireTarget(const FVector& Start, const FVector& Direction, FHitResult& OutHit) const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !IsValid(MountedTurret))
	{
		return false;
	}

	const FVector TraceEnd = Start + Direction * MountedTurret->GetManualRange();

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerPawn);
	QueryParams.AddIgnoredActor(MountedTurret);

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const bool bHit = World->LineTraceSingleByChannel(
		OutHit, 
		Start,
		TraceEnd,
		ECC_Pawn, 
		QueryParams
	);
	
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

	// 보유한 물자실 자원이 없으면 false
	if (PS->GetCarriedSupply() <= 0)
	{
		return false;
	}

	// 포탑 탄창 만탄 채우기
	if (!MountedTurret->RefillAmmoToMax())
	{
		return false;
	}

	PS->ConsumeCarriedSupply(1);
	UE_LOG(LogTRGameplay, Verbose, TEXT("Reload completed, Current Ammo: %d"), MountedTurret->GetCurrentAmmo());
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
void UTRTurretControlComponent::ApplyMountedViewLimits(const ATRTurret* Turret)
{
	APlayerCameraManager* CameraManager = GetLocalCameraManager();
	if (!IsValid(CameraManager) || !IsValid(Turret))
	{
		return;
	}

	// 이미 저장돼 있으면 덮어쓰지 않는다.
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
	if (!IsValid(OwnerPawn))
	{
		return;
	}

	const bool bIsLocal = OwnerPawn->IsLocallyControlled();
	const bool bIsServer = OwnerPawn->HasAuthority();

	if (bIsLocal || bIsServer)
	{
		ATRPlayerCharacter* Character = Cast<ATRPlayerCharacter>(OwnerPawn);
		UCameraComponent* Camera = IsValid(Character) ? Character->GetCameraComponent() : nullptr;
		if (IsValid(Camera))
		{
			if (IsValid(MountedTurret))
			{
				// 터렛 시점으로 전환
				Camera->AttachToComponent(MountedTurret->GetTurretMesh(),
					FAttachmentTransformRules::SnapToTargetNotIncludingScale /* , 소켓명 */);
			}
			else
			{
				// 원래 자리(캐릭터 머리)로 복귀 — 생성자에서 붙인 위치와 동일
				Camera->AttachToComponent(Character->GetMesh(),
					FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("head"));
			}
		}
	}

	// 시야 제한은 로컬 전용이다.
	// 화면을 보는 쪽의 조작감 문제이고, 조작된 회전을 거르는 건 서버의 IsWithinFireArc 다.
	if (!bIsLocal)
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

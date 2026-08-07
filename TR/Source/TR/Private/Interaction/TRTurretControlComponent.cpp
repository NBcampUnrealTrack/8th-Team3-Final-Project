// TRTurretControlComponent.cpp


#include "Interaction/TRTurretControlComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Interaction/TRActionComponent.h"
#include "Camera/CameraComponent.h"
#include "Core/TRPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TRPlayerCharacter.h"
#include "Train/TRTurret.h"

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

void UTRTurretControlComponent::ServerMount_Implementation(ATRTurret* Turret)
{
	if (!IsValid(Turret) || IsMounted() || Turret->IsOccupied() || Turret->IsDestroyed())
	{
		UE_LOG(LogTemp, Log, TEXT("Mount ignored: invalid/occupied/destroyed"));
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn))
	{
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

	if (!IsValid(Turret) || Turret->IsOccupied() || Turret->IsDestroyed())
	{
		UE_LOG(LogTemp, Log, TEXT("Mount failed after delay: target no longer valid"));
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
}

void UTRTurretControlComponent::ServerDismount_Implementation()
{
	ForceDismount();
}

void UTRTurretControlComponent::ForceDismount()
{
	if (!GetOwner()->HasAuthority())
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
		return;
	}

	const float FireInterval = 1.f / FMath::Max(MountedTurret->GetManualFireRate(), 0.01f);   // 0으로 나누기 방지
	const float Now = GetWorld()->GetTimeSeconds();
	if (LastFireTime >= 0.f && Now - LastFireTime < FireInterval)
	{
		return;
	}
	LastFireTime = Now;
	
	FHitResult Hit;
	if (TraceFireTarget(Hit))
	{
		AActor* HitActor = Hit.GetActor();
		if (IsValid(HitActor))
		{
			UGameplayStatics::ApplyDamage(HitActor, MountedTurret->GetManualDamage(), Controller, MountedTurret, nullptr);
		}
	}
	MountedTurret->ConsumeAmmo(1);
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

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerPawn);

	return GetWorld()->LineTraceSingleByChannel(OutHit, ViewLocation, TraceEnd, ECC_Pawn, QueryParams);
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
	return true;
}

void UTRTurretControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ForceDismount();
	Super::EndPlay(EndPlayReason);
}

void UTRTurretControlComponent::OnRep_MountedTurret()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		return;   // 카메라는 이 값을 실제로 "보는" 로컬 플레이어만 처리
	}

	ATRPlayerCharacter* Character = Cast<ATRPlayerCharacter>(OwnerPawn);
	UCameraComponent* Camera = IsValid(Character) ? Character->GetCameraComponent() : nullptr;
	if (!IsValid(Camera))
	{
		return;
	}

	if (IsValid(MountedTurret))
	{
		// 터렛 시점으로 전환
		Camera->AttachToComponent(MountedTurret->GetTurretMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale /* , 소켓명 */);
	}
	else
	{
		// 원래 자리(캐릭터 머리)로 복귀 — 생성자에서 붙인 위치와 동일
		Camera->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("head"));
	}
}

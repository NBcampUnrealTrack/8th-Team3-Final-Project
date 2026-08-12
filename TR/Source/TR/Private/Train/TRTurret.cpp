// TRTurret.cpp


#include "Train/TRTurret.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TRTypes.h"
#include "Core/TRGameState.h"

ATRTurret::ATRTurret()
{
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	RootComponent = TurretMesh;
}

void ATRTurret::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRTurret, Occupant);
	DOREPLIFETIME(ATRTurret, CurrentAmmo);
}

void ATRTurret::BeginPlay()
{
	Super::BeginPlay();

	const FRotator InstallRotation = GetActorRotation();
	InstallYaw = InstallRotation.Yaw;
	InstallPitch = InstallRotation.Pitch;
}

const FTRTurretLevelRow* ATRTurret::GetCurrentLevelRow(const TCHAR* ContextName) const
{
	if (!ensureMsgf(LevelTable, TEXT("%s: LevelTable not assigned"), *GetName()))
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	const ATRGameState* GS = IsValid(World) ? World->GetGameState<ATRGameState>() : nullptr;
	const int32 Level = IsValid(GS) ? GS->GetTurretLevel() : 1;

	const FTRTurretLevelRow* Row = LevelTable->FindRow<FTRTurretLevelRow>(FName(*FString::FromInt(Level)), ContextName);
	ensureMsgf(Row, TEXT("%s: level row %d not found in LevelTable"), *GetName(), Level);
	return Row;
}

float ATRTurret::GetManualRange() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->ManualRange;
	}
	return 0.f;
}

float ATRTurret::GetManualDamage() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->ManualDamage;
	}
	return 0.f;
}

int32 ATRTurret::GetManualAmmoPerResource() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->ManualAmmoPerResource;
	}
	return 0;
}

float ATRTurret::GetMaxHP() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->MaxHP;
	}
	return 0.f;
}

int32 ATRTurret::GetInstallCost() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->InstallCost;
	}
	return 0;
}

float ATRTurret::GetManualFireRate() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->ManualFireRate;
	}
	return 0.f;
}

int32 ATRTurret::GetMaxAmmo() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->AutoMagazine;
	}
	return 0;
}

void ATRTurret::ConsumeAmmo(int32 Amount)
{
	CurrentAmmo = FMath::Max(0, CurrentAmmo - Amount);
}

void ATRTurret::AddAmmo(int32 Amount)
{
	CurrentAmmo = FMath::Clamp(CurrentAmmo + Amount, 0, GetMaxAmmo());
}

bool ATRTurret::IsWithinFireArc(float CameraYaw, float CameraPitch) const
{
	const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(InstallYaw, CameraYaw));
	const float PitchDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(InstallPitch, CameraPitch));
	return YawDelta <= FireArcHalfAngle && PitchDelta <= FireArcHalfAngle;
}

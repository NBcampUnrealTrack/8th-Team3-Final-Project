// TRTurret.cpp


#include "Train/TRTurret.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TRTypes.h"
#include "Core/TRGameState.h"
#include "Debug/TRGameplayLog.h"
#include "GameFramework/Pawn.h"
#include "Interaction/TRTurretControlComponent.h"

ATRTurret::ATRTurret()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(Root);
	// 조준선에 안 걸리게 한다. 탑승은 실내 ConsoleMesh 로만.
	TurretMesh->SetCollisionProfileName(TRCollisionProfile::ExteriorPart);

	// 메시 기본 프로파일(BlockAll)이 Visibility 를 막으므로 상호작용에 그대로 잡힌다.
	ConsoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConsoleMesh"));
	ConsoleMesh->SetupAttachment(Root);
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

	// 액터가 아니라 메시 기준. 루트가 형태 없는 SceneComponent 라 액터 회전은 포탑이 향한 방향이 아니다.
	if (IsValid(TurretMesh))
	{
		const FRotator InstallRotation = TurretMesh->GetComponentRotation();
		InstallYaw = InstallRotation.Yaw;
		InstallPitch = InstallRotation.Pitch;
	}

	// TurretMesh 는 조준선을 무시하므로 상호작용을 잡아주는 건 ConsoleMesh 뿐이다.
	// 메시가 비면 콜리전 형상 자체가 없어 탑승·수리가 조용히 불가능해진다.
	ensureMsgf(IsValid(ConsoleMesh) && ConsoleMesh->GetStaticMesh(),
		TEXT("%s: ConsoleMesh has no static mesh. Interaction is impossible — assign one in the Blueprint."),
		*GetName());

	// CurrentHP 는 서버가 정하고 복제된다. 클라이언트가 붙을 이유가 없다.
	if (!HasAuthority())
	{
		return;
	}

	ATRGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATRGameState>() : nullptr;
	if (IsValid(GS))
	{
		GS->OnTurretLevelUpgraded.AddUObject(this, &ATRTurret::ApplyLevelUpgrade);
	}
}

void ATRTurret::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ATRGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATRGameState>() : nullptr;
	if (IsValid(GS))
	{
		GS->OnTurretLevelUpgraded.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

const FTRTurretLevelRow* ATRTurret::GetCurrentLevelRow(const TCHAR* ContextName) const
{
	// GameState 가 아직 복제되지 않은 클라이언트에서는 잠시 nullptr 이다.
	// CurrentHP·CurrentAmmo 는 복제되고 설치는 서버에서만 하므로 이 창에서 판단이 뒤집히지는 않는다.
	const UWorld* World = GetWorld();
	const ATRGameState* GS = IsValid(World) ? World->GetGameState<ATRGameState>() : nullptr;
	return IsValid(GS) ? GS->GetTurretLevelRow(ContextName) : nullptr;
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

void ATRTurret::OnPartDestroyed()
{
	Super::OnPartDestroyed();
	
	// 아래 어느 경로로 빠지든 점유는 해제한다. 남으면 재설치 후에도 CanBeMounted() 가 false 다.
	APawn* CurrentOccupant = Occupant;
	SetOccupant(nullptr);

	if (!IsValid(CurrentOccupant))
	{
		return;
	}
	
	UTRTurretControlComponent* TurretControl = CurrentOccupant->FindComponentByClass<UTRTurretControlComponent>();
	if (!IsValid(TurretControl))
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("Turret destroyed but occupant %s has no TurretControlComponent"), *CurrentOccupant->GetName());
		return;
	}
	
	TurretControl->ForceDismount();
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

bool ATRTurret::RefillAmmoToMax()
{
	const int32 MaxAmmo = GetMaxAmmo();
	if (MaxAmmo <= 0)
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("RefillAmmoToMax failed: invalid MaxAmmo (%s)"), *GetName());
		return false;
	}
	CurrentAmmo = MaxAmmo;
	return true;
}


bool ATRTurret::IsWithinFireArc(float CameraYaw, float CameraPitch) const
{
	const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(InstallYaw, CameraYaw));
	const float PitchDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(InstallPitch, CameraPitch));
	return YawDelta <= FireArcHalfAngle && PitchDelta <= FireArcHalfAngle;
}

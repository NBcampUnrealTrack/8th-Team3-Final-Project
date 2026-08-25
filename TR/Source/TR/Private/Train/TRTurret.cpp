// TRTurret.cpp


#include "Train/TRTurret.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TRTypes.h"
#include "Core/TRGameState.h"
#include "Debug/TRGameplayLog.h"
#include "GameFramework/Pawn.h"
#include "Interaction/TRTurretControlComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassNavigationSubsystem.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Combat/TRMassCombatSubsystem.h"

ATRTurret::ATRTurret()
{
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

	// 메시가 비어서 콜리전 형상 자체가 없어 탑승·수리가 조용히 불가능해진 상황을 알려주는 로그
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
		GS->OnTurretLevelUpgraded.AddUObject(this, &ATRTurret::HandleLevelUpgraded);
	}
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
	
	StopAutoFire();
	
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

void ATRTurret::OnPartInstalled()
{
	Super::OnPartInstalled();

	// 설치 시 만탄
	RefillAmmoToMax();

	// 설치 완료 = 무인 상태로 살아난 것. 게임 시작 시에도 이 경로로 켜진다.
	StartAutoFire();
}

float ATRTurret::GetManualFireRate() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->ManualFireRate;
	}
	return 0.f;
}

float ATRTurret::GetAutoRange() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->AutoRange;
	}
	return 0.f;
}

float ATRTurret::GetAutoDamagePerShot() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->AutoDamagePerShot;
	}
	return 0.f;
}

float ATRTurret::GetAutoFireRate() const
{
	if (const FTRTurretLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->AutoFireRate;
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

void ATRTurret::SetOccupant(APawn* NewOccupant)
{
	Occupant = NewOccupant;

	if (IsOccupied())
	{
		StopAutoFire();
	}
	else
	{
		StartAutoFire();
	}
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

void ATRTurret::HandleLevelUpgraded(float PrevMaxHP)
{
	// HP 비율 유지는 기반 클래스가 담당한다.
	ApplyLevelUpgrade(PrevMaxHP);

	// AutoFireRate 가 레벨마다 달라지므로 타이머 주기를 다시 잡는다.
	StopAutoFire();
	StartAutoFire();
}

bool ATRTurret::FindNearestEnemy(const FVector& Origin, float Range, FVector& OutTargetLocation) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || Range <= 0.f)
	{
		return false;
	}

	UMassNavigationSubsystem* Navigation = World->GetSubsystem<UMassNavigationSubsystem>();
	if (!IsValid(Navigation))
	{
		return false;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	// 그리드는 2D(XY)다. 사거리 크기의 박스로 후보를 좁히고 실제 거리·각도는 아래에서 본다.
	const FVector Extent(Range, Range, 0.f);
	
	// TFixedAllocator 금지: Query() 는 결과 수에 상한이 없어 넘치면 assert 로 죽는다.
	TArray<FMassNavigationObstacleItem, TInlineAllocator<64>> Candidates;
	Navigation->GetObstacleGrid().Query(FBox(Origin - Extent, Origin + Extent), Candidates);

	// 초기값이 곧 사거리 상한이다. 거리 검사와 최소값 갱신을 한 변수로 처리한다.
	double NearestDistanceSq = FMath::Square(static_cast<double>(Range));
	bool bFound = false;

	for (const FMassNavigationObstacleItem& Candidate : Candidates)
	{
		const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Candidate.Entity);
		const FTREnemyCombatFragment* Combat = EntityManager.GetFragmentDataPtr<FTREnemyCombatFragment>(Candidate.Entity);

		// 전투 프래그먼트가 없으면 적이 아니다.
		if (Transform == nullptr || Combat == nullptr || Combat->Health <= 0.f)
		{
			continue;
		}

		const FVector EnemyLocation = Transform->GetTransform().GetLocation();

		// 3D 거리로 본다. 그리드가 2D 라서 높이는 여기서만 걸러진다.
		const double DistanceSq = FVector::DistSquared(Origin, EnemyLocation);
		if (DistanceSq >= NearestDistanceSq)
		{
			continue;
		}

		// 사각 밖이면 쏠 수 없다. 수동 사격과 같은 검사를 쓴다.
		const FRotator ToEnemy = (EnemyLocation - Origin).Rotation();
		if (!IsWithinFireArc(ToEnemy.Yaw, ToEnemy.Pitch))
		{
			continue;
		}

		NearestDistanceSq = DistanceSq;
		OutTargetLocation = EnemyLocation;
		bFound = true;
	}

	return bFound;
}

bool ATRTurret::IsSplashUnlocked() const
{
	if (SplashMinLevel <= 0 || SplashRadiusRatio <= 0.f || SplashDamageRatio <= 0.f)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ATRGameState* GS = IsValid(World) ? World->GetGameState<ATRGameState>() : nullptr;
	if (!IsValid(GS))
	{
		return false;
	}

	return GS->GetTurretLevel() >= SplashMinLevel;
}

int32 ATRTurret::ApplySplashDamage(const FVector& Center, const FTRMassHitResult& DirectHit,
	float DirectDamage, float SplashRadius)
{
	if (!HasAuthority() || !IsSplashUnlocked())
	{
		return 0;
	}

	const float SplashDamage = DirectDamage * SplashDamageRatio;
	if (SplashDamage <= 0.f || SplashRadius <= 0.f)
	{
		return 0;
	}

	// 연출은 지금도 낸다. 반경을 눈으로 맞춰 두면 피해가 붙는 순간 바로 쓸 수 있다.
	MulticastSplashEffect(Center, SplashRadius);

	// TODO(적/Mass 담당) UTRMassCombatSubsystem::ApplyRadialDamage 가 들어오면 아래
	// 주석을 풀면 이 함수는 완성이다. 그전까지 범위 피해는 들어가지 않는다.
	//
	// UWorld* World = GetWorld();
	// UTRMassCombatSubsystem* Combat = IsValid(World) ? World->GetSubsystem<UTRMassCombatSubsystem>() : nullptr;
	// if (!IsValid(Combat))
	// {
	//     UE_LOG(LogTRGameplay, Warning, TEXT("Splash aborted: TRMassCombatSubsystem not found (%s)"), *GetName());
	//     return 0;
	// }
	// return Combat->ApplyRadialDamage(Center, SplashRadius, SplashDamage, DirectHit.HitEntity);

	UE_LOG(LogTRGameplay, Verbose, TEXT("Splash: %s effect only, ApplyRadialDamage pending (%.0fcm, %.0f each)"),
		*GetName(), SplashRadius, SplashDamage);

	return 0;
}

void ATRTurret::StartAutoFire()
{
	// 자동 사격은 서버 권위다. 파괴됐거나 사람이 탄 포탑은 돌지 않는다.
	if (!HasAuthority() || IsPartDestroyed() || IsOccupied())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	// AutoFireRate 가 0 이면(테이블 행 누락) 0 으로 나눈다. 수동 쪽과 같은 가드.
	const float FireInterval = 1.f / FMath::Max(GetAutoFireRate(), 0.01f);
	World->GetTimerManager().SetTimer(
		AutoFireTimerHandle, 
		this, 
		&ATRTurret::AutoFireTick, 
		FireInterval, 
		true
	);

	UE_LOG(LogTRGameplay, Verbose, TEXT("AutoFire started: %s (%.2fs)"), *GetName(), FireInterval);
}

void ATRTurret::StopAutoFire()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !AutoFireTimerHandle.IsValid())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	UE_LOG(LogTRGameplay, Verbose, TEXT("AutoFire stopped: %s"), *GetName());
}

void ATRTurret::AutoFireTick()
{
	// 타이머로도 막지만, 상태가 바뀌는 순간과 타이머 사이의 한 프레임을 위해 다시 본다.
	if (IsPartDestroyed() || IsOccupied())
	{
		return;
	}

	if (GetCurrentAmmo() <= 0)
	{
		return;
	}

	if (!IsValid(TurretMesh))
	{
		return;
	}

	// 사격이 성립하지 않으면 탐색도 탄약도 쓰지 않는다.
	UWorld* World = GetWorld();
	UTRMassCombatSubsystem* Combat = IsValid(World) ? World->GetSubsystem<UTRMassCombatSubsystem>() : nullptr;
	if (!IsValid(Combat))
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("AutoFire aborted: TRMassCombatSubsystem not found (%s)"), *GetName());
		return;
	}
	
	// 총구는 일단 메시 원점. 모델링이 완성되면 소켓으로 바꾼다.
	const FVector Muzzle = TurretMesh->GetComponentLocation();

	FVector TargetLocation;
	if (!FindNearestEnemy(Muzzle, GetAutoRange(), TargetLocation))
	{
		return;
	}

	ConsumeAmmo(1);

	// 적의 트랜스폼 위치를 그대로 조준한다.
	const float Damage = GetAutoDamagePerShot();
	const FVector FireDirection = (TargetLocation - Muzzle).GetSafeNormal();
	const FTRMassHitResult Hit = Combat->FireHitscan(Muzzle, FireDirection, GetAutoRange(), Damage);

	// 빗나가도 조준한 지점에서 터진다. 자동 사격은 적을 찾아 쏘므로 그 자리가 곧 착탄점이다.
	// Hit.Location 은 빗나가면 0벡터라 그대로 쓸 수 없다.
	const FVector ImpactLocation = Hit.bHit ? Hit.Location : TargetLocation;

	// 연출은 전원이 본다. 빗나가도 총구 화염은 나야 하므로 조건 없이 알린다.
	MulticastFireEffect(ImpactLocation, Hit.bHit);

	ApplySplashDamage(ImpactLocation, Hit, Damage, GetAutoSplashRadius());

	if (!Hit.bHit)
	{
		// 방금 찾은 적을 못 맞힌 상황. 판정 반지름이 좁거나 그 사이에 사라졌다.
		UE_LOG(LogTRGameplay, Verbose, TEXT("AutoFire missed: %s (ammo %d)"), *GetName(), GetCurrentAmmo());
		return;
	}

	UE_LOG(LogTRGameplay, Verbose, TEXT("AutoFire hit: %s -> %s remaining %.0f%s (ammo %d)"),
		*GetName(), LexToString(Hit.EnemyType), Hit.RemainingHealth, Hit.bKilled ? TEXT(" [killed]") : TEXT(""), GetCurrentAmmo());
}

void ATRTurret::MulticastFireEffect_Implementation(FVector HitLocation, bool bEnemyHit)
{
	// 멀티캐스트로 사격 VFX를 스폰하고 Niagara 풀링으로 반환한다.
	if (GetNetMode() != NM_DedicatedServer && IsValid(FireEffect) && IsValid(TurretMesh))
	{
		const bool bAutoDestroy = true;
		const bool bAutoActivate = true;
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			FireEffect,
			TurretMesh,
			FireEffectSocketName,	// 추후 포탑 메시 교체 시 총구 위치로 조정 필요.
			FireEffectLocationOffset,
			FireEffectRotationOffset,
			EAttachLocation::KeepRelativeOffset,
			bAutoDestroy,
			bAutoActivate,
			ENCPoolMethod::AutoRelease);
	}

	const FVector FireOrigin = IsValid(TurretMesh) ? TurretMesh->GetComponentLocation() : GetActorLocation();

#if !UE_BUILD_SHIPPING
	// 적을 맞힌 지점까지만 그린다. SDPG_World 라서 차체에 가려진다 —
	// SDPG_Foreground 로 두면 벽 뒤 사격선까지 실내에 겹쳐 보여 방향을 잘못 읽는다.
	if (bEnemyHit)
	{
		DrawDebugLine(GetWorld(), FireOrigin, HitLocation, FColor::Cyan, false, 0.4f, SDPG_World, 1.f);
	}
#endif

	// TODO: 피탄 이펙트 · 사운드.
}

void ATRTurret::MulticastSplashEffect_Implementation(FVector Center, float Radius)
{
	UWorld* World = GetWorld();

	if (GetNetMode() != NM_DedicatedServer && IsValid(SplashEffect) && IsValid(World))
	{
		// 반경은 레벨·경로마다 달라지므로 이펙트도 그 비율로 키운다.
		const float BaseRadius = FMath::Max(SplashEffectBaseRadius, 1.f);
		const FVector Scale(Radius / BaseRadius);

		const bool bAutoDestroy = true;
		const bool bAutoActivate = true;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			SplashEffect,
			Center,
			FRotator::ZeroRotator,
			Scale,
			bAutoDestroy,
			bAutoActivate,
			ENCPoolMethod::AutoRelease);
	}

#if !UE_BUILD_SHIPPING
	DrawDebugSphere(World, Center, Radius, 16, FColor::Orange, false, 0.6f, SDPG_World, 1.f);
#endif

	// TODO: 폭발 사운드 · 카메라 흔들림.
}


void ATRTurret::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAutoFire(); // AutoFireTimerHandle ClearTimer
	
	ATRGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATRGameState>() : nullptr;
	if (IsValid(GS))
	{
		GS->OnTurretLevelUpgraded.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

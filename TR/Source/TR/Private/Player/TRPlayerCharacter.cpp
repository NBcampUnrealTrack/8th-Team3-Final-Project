//TRPlayerCharacter.cpp


#include "Player/TRPlayerCharacter.h"
#include "Debug/TRGameplayLog.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/TRActionComponent.h"
#include "Interaction/TRInteractionComponent.h"
#include "Interaction/TRTurretControlComponent.h"
#include "Player/TRNameplateComponent.h"
#include "Core/TRGameState.h"
#include "Net/UnrealNetwork.h"
#include "UI/TRPlayerHUD.h"
#include "Train/TRControlConsole.h"
#include "Train/TRInstallablePart.h"
#include "Train/TRSupplyPoint.h"
#include "Train/TRTrain.h"
#include "Train/TRTurret.h"

// Sets default values
ATRPlayerCharacter::ATRPlayerCharacter()
{
 	// 로컬 소유일때만 IMC 등록과 함께 활성화
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 카메라 컴포넌트
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(GetMesh(), FName("head"));
	CameraComp->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	CameraComp->bUsePawnControlRotation = true;
	CameraComp->bEnableFirstPersonFieldOfView = true;
	CameraComp->bEnableFirstPersonScale = true;
	CameraComp->FirstPersonFieldOfView = 70.0f;
	CameraComp->FirstPersonScale = 0.6f;

	WalkEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WalkEffect"));
	WalkEffect->SetupAttachment(RootComponent);
	WalkEffect->bAutoActivate = false;
	
	// ActorComponent
	InteractionComp = CreateDefaultSubobject<UTRInteractionComponent>(TEXT("InteractionComp"));
	ActionComp = CreateDefaultSubobject<UTRActionComponent>(TEXT("ActionComp"));
	TurretControlComp = CreateDefaultSubobject<UTRTurretControlComponent>(TEXT("TurretControlComp"));

	NameplateComp = CreateDefaultSubobject<UTRNameplateComponent>(TEXT("NameplateComp"));
	NameplateComp->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void ATRPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	//이동 이펙트 활성화
	if (IsValid(WalkEffect) && IsValid(WalkEffect->GetAsset()))
	{
		WalkEffect->Activate();
		GetWorld()->GetTimerManager().SetTimer(MoveUpdateTimerHandle, this, &ATRPlayerCharacter::UpdateMoveEffect, 0.2f, true);
		UpdateMoveEffect();
	}

}

void ATRPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveUpdateTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ATRPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRPlayerCharacter, OccupiedConsole);
}

// Look input을 보간해 엔진의 Pitch/Yaw scale 및 제한 경로로 전달.
void ATRPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsLocallyControlled())
	{
		return;
	}

	SmoothedLookInput = FMath::Vector2DInterpTo(SmoothedLookInput, PendingLookInput, DeltaTime, LookInterpolationSpeed);

	if (!FMath::IsNearlyZero(SmoothedLookInput.X))
	{
		AddControllerYawInput(SmoothedLookInput.X);
	}
	if (!FMath::IsNearlyZero(SmoothedLookInput.Y))
	{
		AddControllerPitchInput(SmoothedLookInput.Y);
	}

	//회전 입력 후 소모
	PendingLookInput = FVector2D::ZeroVector;

	SyncControlConsoleWidget();
}

//컨트롤러 설정 - IMC 등록, 틱 활성화 결정
void ATRPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(IMC_Character, 0);
		}

	}

	const bool bShouldTick = IsLocallyControlled();
	SetActorTickEnabled(bShouldTick);

	if (!bShouldTick)
	{
		PendingLookInput = FVector2D::ZeroVector;
		SmoothedLookInput = FVector2D::ZeroVector;
		return;
	}
}

// Called to bind functionality to input
void ATRPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IsValid(IA_Move))
		{
			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ATRPlayerCharacter::Move);
		}
		if (IsValid(IA_Look))
		{
			EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ATRPlayerCharacter::Look);
		}
		if (IsValid(IA_Jump))
		{
			EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &ATRPlayerCharacter::StartJump);
		}
		if (IsValid(IA_Interact))
		{
			EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &ATRPlayerCharacter::OnInteractPressed);
			EIC->BindAction(IA_Interact, ETriggerEvent::Completed, this, &ATRPlayerCharacter::TriggerReleased);
		}
		if (IsValid(IA_Trigger))
		{
			EIC->BindAction(IA_Trigger, ETriggerEvent::Started, this, &ATRPlayerCharacter::TriggerPressed);
			EIC->BindAction(IA_Trigger, ETriggerEvent::Triggered, this, &ATRPlayerCharacter::Trigger);
			EIC->BindAction(IA_Trigger, ETriggerEvent::Completed, this, &ATRPlayerCharacter::TriggerReleased);
		}
		if (IsValid(IA_Reload))
		{
			EIC->BindAction(IA_Reload, ETriggerEvent::Triggered, this, &ATRPlayerCharacter::Reload);
			EIC->BindAction(IA_Reload, ETriggerEvent::Completed, this, &ATRPlayerCharacter::TriggerReleased);
		}
	}
}

// 캐릭터 이동 함수, UCharacterMovementComponent사용으로 RPC 구현 불필요.
void ATRPlayerCharacter::Move(const FInputActionValue& Input)
{
	if (IsValid(TurretControlComp) && TurretControlComp->IsMounted())
	{
		return;
	}
	
	FVector2D InputVector = Input.Get<FVector2D>().GetSafeNormal();
	if (FMath::IsNearlyZero(InputVector.X) && FMath::IsNearlyZero(InputVector.Y))
	{
		return;
	}

	AController* CurrentController = GetController();
	if (!IsLocallyControlled() || !IsValid(CurrentController))
	{
		return;
	}
	
	// 재설치는 관제석 버튼으로 시작하는 액션이라 이동과 무관
	if (IsValid(ActionComp) && ActionComp->IsActionInProgress() && ActionComp->GetCurrentActionType() != ETRActionType::Build)
	{
		ActionComp->ServerCancelAction();
	}
	
	if (!FMath::IsNearlyZero(InputVector.X))
	{
		AddMovementInput(GetActorForwardVector() * InputVector.X);
	}
	if (!FMath::IsNearlyZero(InputVector.Y))
	{
		AddMovementInput(GetActorRightVector() * InputVector.Y);
	}
}

// 캐릭터 회전 함수. Mouse Delta 누적 후 Tick()에서 보간 적용, UCharacterMovementComponent사용으로 RPC 구현 불필요.
void ATRPlayerCharacter::Look(const FInputActionValue& Input)
{
	FVector2D InputVector = Input.Get<FVector2D>();
	if (FMath::IsNearlyZero(InputVector.X) && FMath::IsNearlyZero(InputVector.Y))
	{
		return;
	}

	if (!IsLocallyControlled())
	{
		return;
	}
	PendingLookInput += InputVector;
}

// 캐릭터 점프 함수, UCharacterMovementComponent사용으로 RPC 구현 불필요. 이펙트 전파만 서버 RPC 호출
void ATRPlayerCharacter::StartJump(const FInputActionValue& Input)
{
	if (IsValid(TurretControlComp) && TurretControlComp->IsMounted())
	{
		return;
	}
	
	bool InputBoolean = Input.Get<bool>();
	if (!InputBoolean)
	{
		return;
	}

	AController* CurrentController = GetController();
	if (!IsLocallyControlled() || !IsValid(CurrentController))
	{
		return;
	}

	Jump();

	if (!GetCharacterMovement()->IsFalling())
	{
		ServerPlayJumpLandEffect();
	}
}

//이동속도 및 지면 접촉 여부 이동 이펙트에 전달
void ATRPlayerCharacter::UpdateMoveEffect()
{
	WalkEffect->SetVariableVec3(FName(TEXT("MovementVelocity")), GetVelocity());
	WalkEffect->SetVariableBool(FName(TEXT("IsOnFloor")), !GetCharacterMovement()->IsFalling());
}

//착지 함수, VFX/SFX 임시 연동 (추후 애니메이션 추가시 AnimNotify 기반으로 변경 필요)
void ATRPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (HasAuthority())
	{
		ServerPlayJumpLandEffect();
	}
}

//서버 RPC 점프 및 착지 이팩트 전파 호출
void ATRPlayerCharacter::ServerPlayJumpLandEffect_Implementation()
{
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	if (!IsValid(WalkEffect) || !IsValid(MovementComp))
	{
		return;
	}
	MulticastPlayJumpLandEffect(GetActorLocation() + FVector(0, 0, JumpLandOffset));
}

//멀티캐스트 RPC 점프 및 착지 이펙트 각 클라이언트로 전파
void ATRPlayerCharacter::MulticastPlayJumpLandEffect_Implementation(const FVector& Location)
{
	if (IsValid(JumpLandEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), JumpLandEffect, Location);
	}
}

void ATRPlayerCharacter::OnInteractPressed(const FInputActionValue& Input)
{
	// 점유 중이면 E 는 놓기 전용 — 다른 상호작용으로 새지 않게 한다.
	if (IsOccupyingControlConsole())
	{
		ServerReleaseControlConsole();
		return;
	}

	if (const ATRControlConsole* Console = GetLookedAtControlConsole())
	{
		// 남이 쓰는 중이면 아무 일도 일어나지 않는다. 실제 판정은 서버가 다시 한다.
		if (!Console->IsOccupied())
		{
			ServerRequestControlConsole();
		}
		return;
	}

	Interact(Input);   // 관제석이 아니면 원래대로 서버 RPC 호출
}

void ATRPlayerCharacter::SyncControlConsoleWidget()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	ATRPlayerHUD* PlayerHUD = IsValid(PC) ? Cast<ATRPlayerHUD>(PC->GetHUD()) : nullptr;
	if (!IsValid(PlayerHUD))
	{
		return;
	}

	if (IsOccupyingControlConsole() != PlayerHUD->IsControlConsoleOpen())
	{
		PlayerHUD->ToggleControlConsole();
	}
}

void ATRPlayerCharacter::ServerRequestControlConsole_Implementation()
{
	// 조준 검사는 점유를 시작할 때 한 번만. 이후로는 저장된 상태를 본다.
	ATRControlConsole* Console = GetLookedAtControlConsole();
	if (IsValid(Console) && Console->TryOccupy(this))
	{
		OccupiedConsole = Console;
	}
}

void ATRPlayerCharacter::ServerReleaseControlConsole_Implementation()
{
	if (IsValid(OccupiedConsole))
	{
		OccupiedConsole->Release(this);
	}
	OccupiedConsole = nullptr;
}

ATRControlConsole* ATRPlayerCharacter::GetLookedAtControlConsole() const
{
	return IsValid(InteractionComp) ? Cast<ATRControlConsole>(InteractionComp->GetCurrentTarget()) : nullptr;
}

bool ATRPlayerCharacter::IsOccupyingControlConsole() const
{
	return IsValid(OccupiedConsole);
}

void ATRPlayerCharacter::ServerUpgradeArmor_Implementation()
{
	if (!IsOccupyingControlConsole())
	{
		UE_LOG(LogTRGameplay, Log, TEXT("UpgradeArmor rejected: control console not occupied by this player"));
		return;
	}

	ATRGameState* GameState = GetWorld()->GetGameState<ATRGameState>();
	if (!IsValid(GameState))
	{
		return;
	}

	if (!GameState->TryUpgradeArmorLevel())
	{
		UE_LOG(LogTRGameplay, Log, TEXT("UpgradeArmor rejected: level %d/%d, cost %d, resource %d"),
			GameState->GetArmorLevel(), ATRGameState::MaxArmorLevel,
			GameState->GetArmorUpgradeCost(), GameState->GetMonsterResource());
		return;
	}

	UE_LOG(LogTRGameplay, Verbose, TEXT("UpgradeArmor: level %d (resource left %d)"),
		GameState->GetArmorLevel(), GameState->GetMonsterResource());
}

void ATRPlayerCharacter::ServerUpgradeTurret_Implementation()
{
	if (!IsOccupyingControlConsole())
	{
		UE_LOG(LogTRGameplay, Log, TEXT("UpgradeTurret rejected: control console not occupied by this player"));
		return;
	}

	ATRGameState* GameState = GetWorld()->GetGameState<ATRGameState>();
	if (!IsValid(GameState))
	{
		return;
	}

	if (!GameState->TryUpgradeTurretLevel())
	{
		UE_LOG(LogTRGameplay, Log, TEXT("UpgradeTurret rejected: level %d/%d, cost %d, resource %d"),
			GameState->GetTurretLevel(), ATRGameState::MaxTurretLevel,
			GameState->GetTurretUpgradeCost(), GameState->GetMonsterResource());
		return;
	}

	UE_LOG(LogTRGameplay, Verbose, TEXT("UpgradeTurret: level %d (resource left %d)"),
		GameState->GetTurretLevel(), GameState->GetMonsterResource());
}

// 상호작용 RPC 함수, E키 - 포탑 탑승/하차 vs 물자실 자원 획득
void ATRPlayerCharacter::Interact_Implementation(const FInputActionValue& Input)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(InteractionComp) || !IsValid(ActionComp) || !IsValid(TurretControlComp))
	{
		return;
	}

	if (TurretControlComp->IsMounted())
	{
		TurretControlComp->ServerDismount();
		return;
	}
	
	AActor* Target = InteractionComp->GetCurrentTarget();
	if (!Target)
	{
		UE_LOG(LogTRGameplay, Log, TEXT("Interact ignored: no target"));
		return;
	}

	if (ATRTurret* Turret = Cast<ATRTurret>(Target))
	{
	    TurretControlComp->ServerMount(Turret);
	    return;
	}

	if (Cast<ATRSupplyPoint>(Target))
	{
		ActionComp->ServerStartAction(ETRActionType::Restock, Target);
	}
	else
	{
		UE_LOG(LogTRGameplay, Log, TEXT("Interact ignored: target=%s is not SupplyPoint"), *Target->GetName());
	}
}

// 좌클릭 - 수동사격
void ATRPlayerCharacter::TriggerPressed(const FInputActionValue& Input)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (IsValid(TurretControlComp) && TurretControlComp->IsMounted())
	{
		TurretControlComp->ServerFire();
	}
}

// 트리거 RPC 함수, 좌클릭 - 수리
void ATRPlayerCharacter::Trigger_Implementation(const FInputActionValue& Input)
{
	if (IsValid(TurretControlComp) && TurretControlComp->IsMounted())
	{
		return;
	}
	
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(InteractionComp) || !IsValid(ActionComp))
	{
		return;
	}

	AActor* Target = InteractionComp->GetCurrentTarget();

	if (ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target))
	{
		if (Part->IsPartDestroyed())
		{
			UE_LOG(LogTRGameplay, Log, TEXT("Trigger ignored: %s is destroyed — install from Control Room"), *Part->GetName());
			return;
		}
		ActionComp->ServerStartAction(ETRActionType::Repair, Target);
		return;
	}

	if (Cast<ATRTrain>(Target))
	{
		ActionComp->ServerStartAction(ETRActionType::Repair, Target);
		return;
	}

	UE_LOG(LogTRGameplay, Verbose, TEXT("Trigger ignored: target=%s is not repairable"), Target ? *Target->GetName() : TEXT("null"));
}

// 좌클릭 뗌 - 진행 중이던 Repair 액션 취소
void ATRPlayerCharacter::TriggerReleased_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(ActionComp))
	{
		return;
	}

	// 수리·재장전·보급은 키를 누르고 있는 동안만 진행하므로 떼면 취소한다.
	// 재설치는 관제석 버튼으로 시작하는 액션이라 키와 무관하다 — E 로 관제석을 닫아도 계속 돈다.
	if (ActionComp->GetCurrentActionType() == ETRActionType::Build)
	{
		return;
	}

	ActionComp->ServerCancelAction();
}

void ATRPlayerCharacter::Reload_Implementation(const FInputActionValue& Input)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(InteractionComp) || !IsValid(ActionComp))
	{
		return;
	}

	AActor* Target = InteractionComp->GetCurrentTarget();
	if (!Target)
	{
		UE_LOG(LogTRGameplay, Log, TEXT("Reload ignored: no target"));
		return;
	}

	ActionComp->ServerStartAction(ETRActionType::Reload, Target);
}


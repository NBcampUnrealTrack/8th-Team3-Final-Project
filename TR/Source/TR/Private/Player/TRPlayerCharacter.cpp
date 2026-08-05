// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/TRPlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/TRActionComponent.h"
#include "Interaction/TRInteractionComponent.h"
#include "Train/TRInstallablePart.h"
#include "Train/TRSupplyPoint.h"

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
	
	// ActorComponent
	InteractionComp = CreateDefaultSubobject<UTRInteractionComponent>(TEXT("InteractionComp"));
	ActionComp = CreateDefaultSubobject<UTRActionComponent>(TEXT("ActionComp"));
}

// Called when the game starts or when spawned
void ATRPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
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
			EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &ATRPlayerCharacter::Interact);
		}
		if (IsValid(IA_Trigger))
		{
			EIC->BindAction(IA_Trigger, ETriggerEvent::Triggered, this, &ATRPlayerCharacter::Trigger);
			EIC->BindAction(IA_Trigger, ETriggerEvent::Completed, this, &ATRPlayerCharacter::TriggerReleased);
		}
		if (IsValid(IA_Reload))
		{
			EIC->BindAction(IA_Reload, ETriggerEvent::Started, this, &ATRPlayerCharacter::Reload);
		}
	}
}

// 캐릭터 이동 함수, UCharacterMovementComponent사용으로 RPC 구현 불필요.
void ATRPlayerCharacter::Move(const FInputActionValue& Input)
{
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

	if (ActionComp->IsActionInProgress())
	{
		ActionComp->RequestCancelAction();
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

// 캐릭터 점프 함수, UCharacterMovementComponent사용으로 RPC 구현 불필요.
void ATRPlayerCharacter::StartJump(const FInputActionValue& Input)
{
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
}

// 상호작용 RPC 함수, E키 - 포탑 탑승/하차 vs 물자실 자원 획득
void ATRPlayerCharacter::Interact_Implementation(const FInputActionValue& Input)
{
	if (!HasAuthority())
	{
		return;
	}

	AActor* Target = InteractionComp->GetCurrentTarget();
	if (!Target)
	{
		return;
	}

	// TODO: 터렛 탑승 분기 — TurretControlComponent 생기면 추가
	// if (ATRTurret* Turret = Cast<ATRTurret>(Target))
	// {
	//     TurretControlComp->RequestMount(Turret);
	//     return;
	// }

	if (Cast<ATRSupplyPoint>(Target))
	{
		ActionComp->RequestStartAction(ETRActionType::Restock, Target);
	}
}

// 트리거 RPC 함수, 좌클릭 - 제작/수리 vs 수동사격
void ATRPlayerCharacter::Trigger_Implementation(const FInputActionValue& Input)
{
	if (!HasAuthority())
	{
		return;
	}

	// TODO: 탑승 중이면 사격으로 분기 — TurretControlComponent 생기면 추가
	// if (TurretControlComp->IsMounted())
	// {
	//     TurretControlComp->RequestFire();
	//     return;
	// }

	AActor* Target = InteractionComp->GetCurrentTarget();
	ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target);
	if (!Part)
	{
		return;
	}

	const ETRActionType ActionType = Part->IsDestroyed() ? ETRActionType::Build : ETRActionType::Repair;
	ActionComp->RequestStartAction(ActionType, Target);
}

void ATRPlayerCharacter::TriggerReleased_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	ActionComp->RequestCancelAction();
}

void ATRPlayerCharacter::Reload_Implementation(const FInputActionValue& Input)
{
	if (!HasAuthority())
	{
		return;
	}

	AActor* Target = InteractionComp->GetCurrentTarget();
	if (!Target)
	{
		return;
	}

	ActionComp->RequestStartAction(ETRActionType::Reload, Target);
}


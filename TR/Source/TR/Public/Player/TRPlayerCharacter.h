// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "TRPlayerCharacter.generated.h"

class UCameraComponent;
UCLASS()
class TR_API ATRPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ATRPlayerCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	
	
	//입력 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC_Character;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Jump;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Look;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Interact;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Trigger;
	
protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	virtual void Move(const FInputActionValue& Input);
	virtual void Look(const FInputActionValue& Input);
	virtual void StartJump(const FInputActionValue& Input);

	UFUNCTION(Server, Reliable)
	virtual void Interact(const FInputActionValue& Input);
	virtual void Interact_Implementation(const FInputActionValue& Input);

	UFUNCTION(Server, Unreliable)
	virtual void Trigger(const FInputActionValue& Input);
	virtual void Trigger_Implementation(const FInputActionValue& Input);

	// Local look input smoothing. Character Movement Component sends final ControlRotation to server.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "100.0"))
	float LookInterpolationSpeed = 50.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> CameraComp;

	FVector2D PendingLookInput = FVector2D::ZeroVector;
	FVector2D SmoothedLookInput = FVector2D::ZeroVector;
};

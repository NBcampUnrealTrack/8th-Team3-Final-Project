//TRPlayerCharacter.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "TRPlayerCharacter.generated.h"

class UTRTurretControlComponent;
class UTRActionComponent;
class UTRInteractionComponent;
class UCameraComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class ATRControlConsole;
class ATRPlayerHUD;

UCLASS()
class TR_API ATRPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATRPlayerCharacter();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Camera")
	UCameraComponent* GetCameraComponent() const { return CameraComp; }

	UFUNCTION(BlueprintPure, Category = "Action")
	float GetHealPercent() const { return HealPercent; }

	UFUNCTION(BlueprintPure, Category = "Turret")
	UTRTurretControlComponent* GetTurretControlComponent() const { return TurretControlComp; }

	UFUNCTION(BlueprintPure, Category = "Action")
	UTRActionComponent* GetActionComponent() const { return ActionComp; }

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsOccupyingControlConsole() const;
	
	//--- 입력 액션--- 
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Reload;
	
	// 관제석 팝업에서 호출. 레벨은 GameState가 들고 있어 서버만 올릴 수 있다.
	UFUNCTION(Server, Reliable)
	void ServerUpgradeArmor();
	void ServerUpgradeArmor_Implementation();

	UFUNCTION(Server, Reliable)
	void ServerUpgradeTurret();
	void ServerUpgradeTurret_Implementation();

	// 관제석 점유
	UFUNCTION(Server, Reliable)
	void ServerRequestControlConsole();
	void ServerRequestControlConsole_Implementation();

	UFUNCTION(Server, Reliable)
	void ServerReleaseControlConsole();
	void ServerReleaseControlConsole_Implementation();

protected:
	//--- 플레이어 조작 관련함수 ---
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
	
	UFUNCTION(Server, Unreliable)
	void Reload(const FInputActionValue& Input);
	void Reload_Implementation(const FInputActionValue& Input);
	
	UFUNCTION(Server, Reliable)
	void TriggerReleased();
	void TriggerReleased_Implementation();
	
	void TriggerPressed(const FInputActionValue& Input);
	
	void OnInteractPressed(const FInputActionValue& Input);

	// 지금 조준 중인 관제석. 트레이스는 서버에서도 돌기 때문에 UI 요청 검증에도 쓸 수 있다.
	ATRControlConsole* GetLookedAtControlConsole() const;

	// 팝업은 서버가 정한 점유 상태를 따라 열리고 닫힌다.
	void SyncControlConsoleWidget();
	
	// Local look input smoothing. Character Movement Component sends final ControlRotation to server.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "100.0"))
	float LookInterpolationSpeed = 12.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float HealPercent = 10.f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UTRInteractionComponent> InteractionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action")
	TObjectPtr<UTRActionComponent> ActionComp;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action")
	TObjectPtr<UTRTurretControlComponent> TurretControlComp;
	
	FVector2D PendingLookInput = FVector2D::ZeroVector;
	FVector2D SmoothedLookInput = FVector2D::ZeroVector;

	//--- VFX ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraComponent> WalkEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> JumpLandEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	float JumpLandOffset = -90.f;


	FTimerHandle MoveUpdateTimerHandle;
	void UpdateMoveEffect();

	//이펙트&연출용 함수
	virtual void Landed(const FHitResult& Hit) override;
	UFUNCTION(Server, Unreliable)
	void ServerPlayJumpLandEffect();
	void ServerPlayJumpLandEffect_Implementation();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayJumpLandEffect(const FVector& Location);
	void MulticastPlayJumpLandEffect_Implementation(const FVector& Location);
};

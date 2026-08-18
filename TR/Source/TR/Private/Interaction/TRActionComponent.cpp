// TRActionComponent.cpp


#include "Interaction/TRActionComponent.h"

#include "Debug/TRGameplayLog.h"
#include "Interaction/TRInteractionComponent.h"
#include "Core/TRGameState.h"
#include "Core/TRPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Player/TRPlayerCharacter.h"
#include "Train/TRInstallablePart.h"
#include "Train/TRSupplyPoint.h"
#include "Train/TRTrain.h"
#include "Train/TRTurret.h"
#include "Train/TRControlConsole.h"
#include "GameFramework/GameStateBase.h"

UTRActionComponent::UTRActionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTRActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTRActionComponent, CurrentActionType);
	DOREPLIFETIME(UTRActionComponent, ActionStartServerTime);
	DOREPLIFETIME(UTRActionComponent, CurrentTarget);
	DOREPLIFETIME(UTRActionComponent, bRepairEffectActive);
	DOREPLIFETIME(UTRActionComponent, bSupplyEffectActive);
}

bool UTRActionComponent::ServerStartAction_Validate(ETRActionType ActionType, AActor* Target)
{
	// 조작된 enum 값 방어
	if (!ActionDefs.Contains(ActionType))
	{
		return false;
	}
	return true;
}

UTRActionComponent* UTRActionComponent::FindBuildingComponent(UWorld* World)
{
	// GameState가 진행 중인 재설치를 들고 있어 순회 없이 바로 답한다.
	const ATRGameState* GameState = IsValid(World) ? World->GetGameState<ATRGameState>() : nullptr;
	return IsValid(GameState) ? GameState->GetBuildingComponent() : nullptr;
}

void UTRActionComponent::SetCurrentAction(ETRActionType NewType, AActor* NewTarget)
{
	CurrentActionType = NewType;
	CurrentTarget = NewTarget;
	bRepairEffectActive = NewType == ETRActionType::Repair;
	bSupplyEffectActive = NewType == ETRActionType::Reload;
	RefreshOwnerActionEffects();

	ATRGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ATRGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return;
	}

	// 재설치 표식은 이 함수에서만 세우고 내린다. 내가 세운 것만 내린다.
	if (NewType == ETRActionType::Build)
	{
		GameState->SetBuildingComponent(this);
	}
	else if (GameState->GetBuildingComponent() == this)
	{
		GameState->SetBuildingComponent(nullptr);
	}
}

//두 RepNotify가 어느 순서로 와도 마지막으로 복제된 상태 조합을 한 번에 적용한다.
void UTRActionComponent::RefreshOwnerActionEffects() const
{
	if (ATRPlayerCharacter* Character = Cast<ATRPlayerCharacter>(GetOwner()))
	{
		Character->SetActionEffectsActive(bRepairEffectActive, bSupplyEffectActive);
	}
}

void UTRActionComponent::OnRep_RepairEffectActive()
{
	RefreshOwnerActionEffects();
}

void UTRActionComponent::OnRep_SupplyEffectActive()
{
	RefreshOwnerActionEffects();
}

void UTRActionComponent::ServerStartAction_Implementation(ETRActionType ActionType, AActor* Target)
{
	// 이미 다른 액션 진행 중이면 무시 (동시 2개 방지)
	if (CurrentActionType != ETRActionType::None)
	{
		return;
	}

	// 재설치는 열차 전체에서 한 번에 하나만 한다. 다른 플레이어가 짓는 중이면 거부
	if (ActionType == ETRActionType::Build && FindBuildingComponent(GetWorld()))
	{
		UE_LOG(LogTRGameplay, Log, TEXT("Build rejected: another build is already in progress"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Validate에서 이미 존재 확인했으므로 재확인 불필요
	const FTRActionDef& Def = ActionDefs[ActionType];

	if (!ValidateActionTarget(ActionType, Target))
	{
		return;
	}

	// 자원 체크 (제작=몬스터 자원, 수리/재장전=물자실 자원, 재보급=소모 없음)
	if (ActionType == ETRActionType::Restock)
	{
		// 소모 없음 - 체크 스킵 (인벤토리 공간 여부는 ValidateActionTarget에서 처리)
	}
	else if (RequiresMonsterResource(ActionType))
	{
		const ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target);
		if (!IsValid(Part))
		{
			UE_LOG(LogTRGameplay, Log, TEXT("StartAction rejected: target is not an installable part"));
			return;
		}
		const int32 InstallCost = Part ? Part->GetInstallCost() : 0;

		ATRGameState* GS = GetWorld()->GetGameState<ATRGameState>();
		if (!GS || GS->GetMonsterResource() < InstallCost)
		{
			return;
		}
	}
	else
	{
		APawn* OwnerPawn = Cast<APawn>(GetOwner());
		ATRPlayerState* PS = OwnerPawn ? Cast<ATRPlayerState>(OwnerPawn->GetPlayerState()) : nullptr;
		if (!PS)
		{
			return;
		}

		if (PS->GetCarriedSupply() < Def.ResourceAmount)
		{
			return;
		}
	}
	
	SetCurrentAction(ActionType, Target);
	ActionStartServerTime = World->GetTimeSeconds();

	World->GetTimerManager().SetTimer(
		ActionTimerHandle,
		this,
		&UTRActionComponent::CompleteAction,
		Def.CycleDuration,
		false
	);
	UE_LOG(LogTRGameplay, Verbose, TEXT("Action Started: %.1fs"), Def.CycleDuration);
}

void UTRActionComponent::ServerCancelAction_Implementation()
{
	if (CurrentActionType == ETRActionType::None)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimerHandle);
	}
	SetCurrentAction(ETRActionType::None, nullptr);
}

void UTRActionComponent::CompleteAction()
{
	const FTRActionDef* Def = ActionDefs.Find(CurrentActionType);
	AActor* Target = CurrentTarget.Get();

	if (Def && Target)
	{
		APawn* OwnerPawn = Cast<APawn>(GetOwner());
		ATRPlayerState* PS = OwnerPawn ? Cast<ATRPlayerState>(OwnerPawn->GetPlayerState()) : nullptr;

		switch (CurrentActionType)
		{
			case ETRActionType::Repair:
			{
				ATRPlayerCharacter* Character = Cast<ATRPlayerCharacter>(OwnerPawn);
				const float HealPercent = IsValid(Character) ? Character->GetHealPercent() : 0.f;
				bool bRepaired = false;
				if (ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target))
				{
					if (Part->CanBeRepaired())
					{
						Part->ApplyRepair(Part->GetMaxHP() * (HealPercent / 100.f));
						UE_LOG(LogTRGameplay, Verbose, TEXT("Repair complete: %s HP %.0f / %.0f"), *Part->GetName(), Part->GetCurrentHP(), Part->GetMaxHP());
						bRepaired = true;
					}
					else
					{
						UE_LOG(LogTRGameplay, Verbose, TEXT("Repair skipped: %s no longer repairable"), *Part->GetName());
					}
				}
				else if (ATRTrain* Train = Cast<ATRTrain>(Target))
				{
					if (Train->CanBeRepaired())
					{
						Train->ApplyRepair(Train->GetMaxHullHealth() * (HealPercent / 100.f));
						UE_LOG(LogTRGameplay, Verbose, TEXT("Repair complete: Hull HP %.0f / %.0f"), Train->GetCurrentHullHealth(), Train->GetMaxHullHealth());
						bRepaired = true;
					}
					else
					{
						UE_LOG(LogTRGameplay, Verbose, TEXT("Repair skipped: Train no longer repairable"));
					}
				}
				if (bRepaired && PS)
				{
					PS->ConsumeCarriedSupply(Def->ResourceAmount);
				}
				break;
			}
			case ETRActionType::Reload:
			{
				bool bReloaded = false;
				if (ATRTurret* Turret = Cast<ATRTurret>(Target))
				{
					if (Turret->CanBeReloaded() && Turret->RefillAmmoToMax())
					{
						UE_LOG(LogTRGameplay, Verbose, TEXT("Reload complete: %s Ammo %d / %d"), *Turret->GetName(), Turret->GetCurrentAmmo(), Turret->GetMaxAmmo());
						bReloaded = true;
					}
					else
					{
						UE_LOG(LogTRGameplay, Verbose, TEXT("Reload skipped: %s no longer reloadable"), *Turret->GetName());
					}
				}
				if (bReloaded && PS)
				{
					PS->ConsumeCarriedSupply(Def->ResourceAmount);
				}
				break;
			}
			case ETRActionType::Build:
			{
				if (ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target))
				{
					const int32 InstallCost = Part->GetInstallCost();
					
					// TryConsumeMonsterResource 는 검사와 차감을 함께 한다. 여기서 이미 자원이 빠지므로
					// 아래에서 다시 차감하면 안 된다.
					ATRGameState* GameState = GetWorld()->GetGameState<ATRGameState>();
					if (!IsValid(GameState) || !GameState->TryConsumeMonsterResource(InstallCost))
					{
						UE_LOG(LogTRGameplay, Log, TEXT("Build aborted: not enough resource for %s (cost %d)"), *Part->GetName(), InstallCost);
						break;
					}
					Part->ApplyInstall();
					UE_LOG(LogTRGameplay, Verbose, TEXT("Build complete: %s (cost %d, resource left %d)"), *Part->GetName(), InstallCost, GameState->GetMonsterResource());
				}
				break;
			}
			case ETRActionType::Restock:
			{
				if (PS)
				{
					PS->AddCarriedSupply(Def->ResourceAmount);
					UE_LOG(LogTRGameplay, Verbose, TEXT("Restock complete: CarriedSupply %d / %d"), PS->GetCarriedSupply(), ATRPlayerState::MaxCarriedSupply);
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}

	SetCurrentAction(ETRActionType::None, nullptr);
}

float UTRActionComponent::GetActionProgress01() const
{
	if (CurrentActionType == ETRActionType::None)
	{
		return 0.f;
	}

	const FTRActionDef* Def = ActionDefs.Find(CurrentActionType);
	if (!Def || Def->CycleDuration <= 0.f)
	{
		return 0.f;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	if (!IsValid(GameState))
	{
		return 0.f;
	}

	const float Elapsed = GameState->GetServerWorldTimeSeconds() - ActionStartServerTime;
	return FMath::Clamp(Elapsed / Def->CycleDuration, 0.f, 1.f);
}

bool UTRActionComponent::ValidateActionTarget(ETRActionType ActionType, AActor* Target) const
{
	if (!Target)
	{
		UE_LOG(LogTRGameplay, Log, TEXT("ValidateActionTarget failed: target is null"));
		return false;
	}

	// 클라이언트가 임의의 액터를 Target으로 넘길 수 있으므로, 실제 상호작용 대상과 일치하는지 서버에서 재검증
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const UTRInteractionComponent* Interaction = IsValid(OwnerPawn) ? OwnerPawn->FindComponentByClass<UTRInteractionComponent>() : nullptr;
	if (!IsValid(Interaction))
	{
		UE_LOG(LogTRGameplay, Log, TEXT("ValidateActionTarget failed: no InteractionComponent"));
		return false;
	}

	if (ActionType == ETRActionType::Build)
	{
		// Build(재설치)는 관제석 UI에서 원격으로 대상을 고르므로, 실제 조준 대상은 파츠가 아니라 ControlConsole이어야 한다
		if (!Cast<ATRControlConsole>(Interaction->GetCurrentTarget()))
		{
			UE_LOG(LogTRGameplay, Log, TEXT("ValidateActionTarget failed: Build requires looking at ControlConsole"));
			return false;
		}
	}
	else if (Interaction->GetCurrentTarget() != Target)
	{
		UE_LOG(LogTRGameplay, Log, TEXT("ValidateActionTarget failed: target=%s is not current interaction target"), *Target->GetName());
		return false;
	}

	bool bValid = false;
	switch (ActionType)
	{
		case ETRActionType::Repair:
			{
				if (const ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target))
				{
					bValid = Part->CanBeRepaired();
				}
				else if (const ATRTrain* Train = Cast<ATRTrain>(Target))
				{
					bValid = Train->CanBeRepaired();
				}
				break;
			}
		case ETRActionType::Build:
			{
				const ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target);
				bValid = Part && Part->IsPartDestroyed();
				break;
			}
		case ETRActionType::Reload:
			{
				const ATRTurret* Turret = Cast<ATRTurret>(Target);
				bValid = Turret && Turret->CanBeReloaded();
				break;
			}
		case ETRActionType::Restock:
			{
				const ATRPlayerState* PS = IsValid(OwnerPawn) ? Cast<ATRPlayerState>(OwnerPawn->GetPlayerState()) : nullptr;
				bValid = Cast<ATRSupplyPoint>(Target) != nullptr && IsValid(PS) && PS->HasSupplySpace();
				break;
			}
		default:
			break;
	}

	if (!bValid)
	{
		UE_LOG(LogTRGameplay, Log, TEXT("ValidateActionTarget failed: action=%d target=%s"), static_cast<int32>(ActionType), *Target->GetName());
	}
	return bValid;
}

void UTRActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimerHandle);
	}

	bRepairEffectActive = false;
	bSupplyEffectActive = false;
	RefreshOwnerActionEffects();

	Super::EndPlay(EndPlayReason);
}

// TRActionComponent.cpp


#include "Interaction/TRActionComponent.h"

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

UTRActionComponent::UTRActionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTRActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTRActionComponent, CurrentActionType);
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

void UTRActionComponent::ServerStartAction_Implementation(ETRActionType ActionType, AActor* Target)
{
	// 이미 다른 액션 진행 중이면 무시 (동시 2개 방지)
	if (CurrentActionType != ETRActionType::None)
	{
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
		// 소모 없음 — 체크 스킵 (인벤토리 공간 여부는 ValidateActionTarget에서 처리)
	}
	else if (RequiresMonsterResource(ActionType))
	{
		ATRGameState* GS = GetWorld()->GetGameState<ATRGameState>();
		if (!GS || GS->GetMonsterResource() < Def.ResourceAmount)
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
	
	CurrentActionType = ActionType;
	CurrentTarget = Target;

	World->GetTimerManager().SetTimer(
		ActionTimerHandle,
		this,
		&UTRActionComponent::CompleteAction,
		Def.CycleDuration,
		false
	);
	UE_LOG(LogTemp, Warning, TEXT("Action Started: %.1fs"), Def.CycleDuration);
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
	CurrentActionType = ETRActionType::None;
	CurrentTarget = nullptr;
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

					if (ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target))
					{
						Part->ApplyRepair(Part->GetMaxHP() * HealPercent);
					}
					else if (ATRTrain* Train = Cast<ATRTrain>(Target))
					{
						Train->ApplyRepair(Train->GetMaxHullHealth() * HealPercent);
					}

					if (PS)
					{
						PS->ConsumeCarriedSupply(Def->ResourceAmount);
					}
					break;
				}
			case ETRActionType::Reload:
				{
					if (ATRTurret* Turret = Cast<ATRTurret>(Target))
					{
						Turret->AddAmmo(Def->ResourceAmount * Turret->GetManualAmmoPerResource());
					}
					if (PS)
					{
						PS->ConsumeCarriedSupply(Def->ResourceAmount);
					}
					break;
				}
			case ETRActionType::Build:
				{
					if (ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target))
					{
						Part->ApplyInstall();
					}
					if (ATRGameState* GS = GetWorld()->GetGameState<ATRGameState>())
					{
						GS->ConsumeMonsterResource(Def->ResourceAmount);
					}
					break;
				}
			case ETRActionType::Restock:
				{
					if (PS)
					{
						PS->AddCarriedSupply(Def->ResourceAmount);
					}
					break;
				}
			default:
				break;
		}
	}

	CurrentActionType = ETRActionType::None;
	CurrentTarget = nullptr;
}

float UTRActionComponent::GetActionProgress01() const
{
	// TODO: UI 붙을 때 구현 (서버 전용 타이머라 클라 표시하려면 시작 시각 복제 또는 별도 방식 필요 — 그때 다시 설계)
	return 0.f;
}

bool UTRActionComponent::ValidateActionTarget(ETRActionType ActionType, AActor* Target) const
{
	if (!Target)
	{
		UE_LOG(LogTemp, Log, TEXT("ValidateActionTarget failed: target is null"));
		return false;
	}

	bool bValid = false;
	switch (ActionType)
	{
		case ETRActionType::Repair:
			{
				if (const ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target))
				{
					bValid = !Part->IsDestroyed() && !Part->IsFullHP();
				}
				else if (const ATRTrain* Train = Cast<ATRTrain>(Target))
				{
					bValid = !Train->IsDestroyed() && !Train->IsFullHP();
				}
				break;
			}
		case ETRActionType::Build:
			{
				const ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target);
				bValid = Part && Part->IsDestroyed();
				break;
			}
		case ETRActionType::Reload:
			{
				const ATRTurret* Turret = Cast<ATRTurret>(Target);
				bValid = Turret && Turret->GetCurrentAmmo() < Turret->GetMaxAmmo();
				break;
			}
		case ETRActionType::Restock:
			{
				bValid = Cast<ATRSupplyPoint>(Target) != nullptr;
				break;
			}
		default:
			break;
	}

	if (!bValid)
	{
		UE_LOG(LogTemp, Log, TEXT("ValidateActionTarget failed: action=%d target=%s"), static_cast<int32>(ActionType), *Target->GetName());
	}
	return bValid;
}

void UTRActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

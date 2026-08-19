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
	DOREPLIFETIME(UTRActionComponent, EffectiveCycleDuration);
	DOREPLIFETIME(UTRActionComponent, CurrentTarget);
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
	RefreshOwnerActionEffect();

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

void UTRActionComponent::RefreshOwnerActionEffect()
{
	if (ATRPlayerCharacter* Character = Cast<ATRPlayerCharacter>(GetOwner()))
	{
		Character->SetActionEffect(CurrentActionType);
	}
}

void UTRActionComponent::OnRep_CurrentActionType()
{
	RefreshOwnerActionEffect();
}

int32 UTRActionComponent::CountConcurrentWorkers(ETRActionType ActionType, const AActor* Target) const
{
	if (ActionType == ETRActionType::None || !IsValid(Target))
	{
		return 0;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	if (!IsValid(GameState))
	{
		return 0;
	}

	int32 Count = 0;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APawn* Pawn = IsValid(PlayerState) ? PlayerState->GetPawn() : nullptr;
		const UTRActionComponent* Action = IsValid(Pawn) ? Pawn->FindComponentByClass<UTRActionComponent>() : nullptr;
		if (!IsValid(Action))
		{
			continue;
		}

		// 대상과 작업이 모두 같아야 센다.
		if (Action->CurrentActionType == ActionType && Action->CurrentTarget.Get() == Target)
		{
			++Count;
		}
	}

	return Count;
}

float UTRActionComponent::ResolveCycleDuration(ETRActionType ActionType, const FTRActionDef& Def, int32 WorkerCount) const
{
	// 협동 보너스는 수리 전용
	if (ActionType != ETRActionType::Repair || RepairConcurrentSpeedMultipliers.IsEmpty())
	{
		return Def.CycleDuration;
	}

	const int32 Index = FMath::Clamp(WorkerCount - 1, 0, RepairConcurrentSpeedMultipliers.Num() - 1);
	const float Multiplier = RepairConcurrentSpeedMultipliers[Index];

	// 0 이하 배수 방지
	if (Multiplier <= 0.f)
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("RepairConcurrentSpeedMultipliers[%d] is %.2f — must be positive. Using base duration."),
			Index, Multiplier);
		return Def.CycleDuration;
	}

	return Def.CycleDuration / Multiplier;
}

void UTRActionComponent::RecomputeCycleTiming()
{
	if (CurrentActionType == ETRActionType::None)
	{
		return;
	}

	const FTRActionDef* Def = ActionDefs.Find(CurrentActionType);
	UWorld* World = GetWorld();
	if (Def == nullptr || !IsValid(World))
	{
		return;
	}

	const int32 WorkerCount = CountConcurrentWorkers(CurrentActionType, CurrentTarget.Get());
	const float NewDuration = ResolveCycleDuration(CurrentActionType, *Def, WorkerCount);
	if (NewDuration <= 0.f || FMath::IsNearlyEqual(NewDuration, EffectiveCycleDuration))
	{
		return;
	}

	// 이미 진행한 몫은 보존한다. 진행률을 축으로 잡아야 진행 바가 튀지 않는다.
	const float Elapsed01 = GetActionProgress01();

	// 시작 시각을 과거로 역산해 덮는다.
	const float Now = World->GetTimeSeconds();
	ActionStartServerTime = Now - NewDuration * Elapsed01;
	EffectiveCycleDuration = NewDuration;

	// rate 0 이하 방지
	const float Remaining = FMath::Max(NewDuration * (1.f - Elapsed01), KINDA_SMALL_NUMBER);
	World->GetTimerManager().SetTimer(
		ActionTimerHandle,
		this,
		&UTRActionComponent::CompleteAction,
		Remaining,
		false
	);

	UE_LOG(LogTRGameplay, Verbose, TEXT("Action retimed: %.2fs (workers %d, progress %.0f%%, remaining %.2fs)"),
		NewDuration, WorkerCount, Elapsed01 * 100.f, Remaining);
}

void UTRActionComponent::RefreshConcurrentWorkers(ETRActionType ActionType, AActor* Target)
{
	if (ActionType == ETRActionType::None || !IsValid(Target))
	{
		return;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	if (!IsValid(GameState))
	{
		return;
	}

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APawn* Pawn = IsValid(PlayerState) ? PlayerState->GetPawn() : nullptr;
		UTRActionComponent* Action = IsValid(Pawn) ? Pawn->FindComponentByClass<UTRActionComponent>() : nullptr;
		if (!IsValid(Action) || Action == this)
		{
			continue;
		}

		if (Action->CurrentActionType == ActionType && Action->CurrentTarget.Get() == Target)
		{
			Action->RecomputeCycleTiming();
		}
	}
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

	// 인원 집계에 내가 포함돼야 하므로 SetCurrentAction 뒤에 센다.
	const int32 WorkerCount = CountConcurrentWorkers(ActionType, Target);
	EffectiveCycleDuration = ResolveCycleDuration(ActionType, Def, WorkerCount);
	ActionStartServerTime = World->GetTimeSeconds();

	World->GetTimerManager().SetTimer(
		ActionTimerHandle,
		this,
		&UTRActionComponent::CompleteAction,
		FMath::Max(EffectiveCycleDuration, KINDA_SMALL_NUMBER),
		false
	);

	// 먼저 붙어 있던 사람들도 늘어난 인원만큼 빨라져야 한다.
	RefreshConcurrentWorkers(ActionType, Target);

	UE_LOG(LogTRGameplay, Verbose, TEXT("Action Started: %.2fs (workers %d, base %.1fs)"),
		EffectiveCycleDuration, WorkerCount, Def.CycleDuration);
}

void UTRActionComponent::ServerCancelAction_Implementation()
{
	if (CurrentActionType == ETRActionType::None)
	{
		return;
	}

	// 내가 빠진 뒤 남은 인원을 다시 세야 하므로 지우기 전에 잡아둔다.
	const ETRActionType PrevActionType = CurrentActionType;
	AActor* PrevTarget = CurrentTarget.Get();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimerHandle);
	}
	SetCurrentAction(ETRActionType::None, nullptr);
	EffectiveCycleDuration = 0.f;

	// 남은 사람들은 줄어든 인원만큼 느려진다.
	RefreshConcurrentWorkers(PrevActionType, PrevTarget);
}

void UTRActionComponent::CompleteAction()
{
	// 아래에서 상태를 지운 뒤 남은 인원에게 알려야 하므로 미리 잡아둔다.
	const ETRActionType CompletedType = CurrentActionType;
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
	EffectiveCycleDuration = 0.f;

	// 내 사이클이 끝나 인원이 줄었다. 계속 붙어 있는 사람들의 시간을 다시 잰다.
	RefreshConcurrentWorkers(CompletedType, Target);
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

	// 인원 배수가 적용된 시간으로 잰다. 복제가 도착하기 전 한 프레임은 0이므로 기준 시간으로 물러난다.
	const float Duration = (EffectiveCycleDuration > 0.f) ? EffectiveCycleDuration : Def->CycleDuration;

	const float Elapsed = GameState->GetServerWorldTimeSeconds() - ActionStartServerTime;
	return FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
}

bool UTRActionComponent::ValidateActionTarget(ETRActionType ActionType, AActor* Target) const
{
	if (!Target)
	{
		UE_LOG(LogTRGameplay, Log, TEXT("ValidateActionTarget failed: target is null"));
		return false;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());

	if (ActionType == ETRActionType::Build)
	{
		// Build(재설치)는 관제석 UI에서 원격으로 대상을 고르므로 조준을 볼 이유가 없다.
		const ATRPlayerCharacter* Character = Cast<ATRPlayerCharacter>(OwnerPawn);
		if (!IsValid(Character) || !Character->IsOccupyingControlConsole())
		{
			UE_LOG(LogTRGameplay, Log, TEXT("ValidateActionTarget failed: Build requires occupying the ControlConsole"));
			return false;
		}
	}
	else
	{
		// 클라이언트가 임의의 액터를 Target으로 넘길 수 있으므로, 실제 상호작용 대상과 일치하는지 서버에서 재검증
		const UTRInteractionComponent* Interaction = IsValid(OwnerPawn) ? OwnerPawn->FindComponentByClass<UTRInteractionComponent>() : nullptr;
		if (!IsValid(Interaction))
		{
			UE_LOG(LogTRGameplay, Log, TEXT("ValidateActionTarget failed: no InteractionComponent"));
			return false;
		}

		if (Interaction->GetCurrentTarget() != Target)
		{
			UE_LOG(LogTRGameplay, Log, TEXT("ValidateActionTarget failed: target=%s is not current interaction target"), *Target->GetName());
			return false;
		}
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

	const AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		// 접속이 끊기거나 폰이 파괴된 경우 알린다.
		const ETRActionType PrevActionType = CurrentActionType;
		AActor* PrevTarget = CurrentTarget.Get();

		SetCurrentAction(ETRActionType::None, nullptr);
		EffectiveCycleDuration = 0.f;

		RefreshConcurrentWorkers(PrevActionType, PrevTarget);
	}

	// 파괴 중에는 마지막 복제가 도착하지 않을 수 있으므로 각 머신에서 즉시 끈다.
	if (ATRPlayerCharacter* Character = Cast<ATRPlayerCharacter>(GetOwner()))
	{
		Character->SetActionEffect(ETRActionType::None);
	}

	Super::EndPlay(EndPlayReason);
}

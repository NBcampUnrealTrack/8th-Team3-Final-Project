// TRActionComponent.cpp


#include "Interaction/TRActionComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Train/TRInstallablePart.h"
#include "Train/TRSupplyPoint.h"
#include "Train/TRTurret.h"

// Sets default values for this component's properties
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

void UTRActionComponent::RequestStartAction(ETRActionType ActionType, AActor* Target)
{
	// 클라이언트 로컬 예측 없음. 바로 서버에 위임.
	ServerStartAction(ActionType, Target);
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
		// TODO: GameState에 MonsterResource 필드 선언 후 실제 이름으로 교체
		// ATRGameState* GS = GetWorld()->GetGameState<ATRGameState>();
		// if (!GS || GS->MonsterResource < Def.ResourceAmount)
		// {
		//     return;
		// }
	}
	else
	{
		APawn* OwnerPawn = Cast<APawn>(GetOwner());
		APlayerState* PS = OwnerPawn ? OwnerPawn->GetPlayerState() : nullptr;
		if (!PS)
		{
			return;
		}
		// TODO: PlayerState에 CarriedSupply 필드 선언 후 실제 이름으로 교체
		// if (PS->CarriedSupply < Def.ResourceAmount)
		// {
		//     return;
		// }
	}
	
	CurrentActionType = ActionType;
	CurrentTarget = Target;

	GetWorld()->GetTimerManager().SetTimer(
		ActionTimerHandle,
		this,
		&UTRActionComponent::CompleteAction,
		Def.CycleDuration,
		false
	);
	UE_LOG(LogTemp, Warning, TEXT("Action Started: %.1fs"), Def.CycleDuration);
}

void UTRActionComponent::RequestCancelAction()
{
	ServerCancelAction();
}

void UTRActionComponent::ServerCancelAction_Implementation()
{
	if (CurrentActionType == ETRActionType::None)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(ActionTimerHandle);
	CurrentActionType = ETRActionType::None;
	CurrentTarget = nullptr;
}

void UTRActionComponent::CompleteAction()
{
	UE_LOG(LogTemp, Warning, TEXT("Action Completed"));
	
	const FTRActionDef* Def = ActionDefs.Find(CurrentActionType);
	AActor* Target = CurrentTarget.Get();

	if (Def && Target)
	{
		switch (CurrentActionType)
		{
		case ETRActionType::Repair:
			// TODO: Target 내구도 회복
			// TODO: PS->CarriedSupply -= Def->ResourceAmount;
			break;
		case ETRActionType::Reload:
			// TODO: Target 탄약 보충
			// TODO: PS->CarriedSupply -= Def->ResourceAmount;
			break;
		case ETRActionType::Build:
			{
				if (ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target))
				{
					Part->ApplyInstall();
				}
				// TODO: GS->MonsterResource -= Def->ResourceAmount;
				break;
			}
		case ETRActionType::Restock:
			// TODO: PS->CarriedSupply += Def->ResourceAmount;
			break;
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
			const ATRInstallablePart* Part = Cast<ATRInstallablePart>(Target);
			bValid = Part && !Part->IsDestroyed() && !Part->IsFullHP();
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
			// TODO: 탄창 꽉 찬 상태면 재장전 불가 — ATRTurret에 탄약 필드 생기면 추가
			bValid = Cast<ATRTurret>(Target) != nullptr;
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

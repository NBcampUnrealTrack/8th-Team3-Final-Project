// TRGameState.cpp


#include "Core/TRGameState.h"
#include "Interaction/TRActionComponent.h"
#include "Net/UnrealNetwork.h"
#include "Core/TRPlayerState.h"

void ATRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRGameState, MonsterResource);
	DOREPLIFETIME(ATRGameState, ArmorLevel);
	DOREPLIFETIME(ATRGameState, TurretLevel);
	DOREPLIFETIME(ATRGameState, BuildingComponent);
	DOREPLIFETIME(ATRGameState, bMatchStarted);
}

UTRActionComponent* ATRGameState::GetBuildingComponent() const
{
	// 짓던 플레이어가 나가면 컴포넌트가 사라지므로 여기서 걸러 준다.
	UTRActionComponent* Component = BuildingComponent.Get();
	return IsValid(Component) ? Component : nullptr;
}

void ATRGameState::SetBuildingComponent(UTRActionComponent* InComponent)
{
	if (!HasAuthority())
	{
		return;
	}

	BuildingComponent = InComponent;
}

bool ATRGameState::TryConsumeMonsterResource(int32 Amount)
{
	if (!HasAuthority())
	{
		return false;
	}
	
	if (MonsterResource < Amount)
	{
		return false;
	}
	MonsterResource -= Amount;
	return true;
}

bool ATRGameState::UpgradeArmorLevel()
{
	if (!HasAuthority())
	{
		return false;
	}
	
	if (ArmorLevel >= MaxArmorLevel)
	{
		return false;
	}
	++ArmorLevel;
	return true;
}

bool ATRGameState::UpgradeTurretLevel()
{
	if (!HasAuthority())
	{
		return false;
	}
	
	if (TurretLevel >= MaxTurretLevel)
	{
		return false;
	}
	++TurretLevel;
	return true;
}

int32 ATRGameState::GetReadyPlayerCount() const
{
	int32 Count = 0;
	for (const APlayerState* State : PlayerArray)
	{
		const ATRPlayerState* TRState = Cast<ATRPlayerState>(State);
		if (TRState != nullptr && !TRState->IsHost() && TRState->IsReady())
		{
			++Count;
		}
	}

	return Count;
}

int32 ATRGameState::GetRequiredReadyCount() const
{
	int32 Count = 0;
	for (const APlayerState* State : PlayerArray)
	{
		const ATRPlayerState* TRState = Cast<ATRPlayerState>(State);
		if (TRState != nullptr && !TRState->IsHost())
		{
			++Count;
		}
	}

	return Count;
}

bool ATRGameState::CanStartMatch() const
{
	// 혼자면 필요 인원이 0 이라 곧바로 시작할 수 있다.
	return !bMatchStarted && GetReadyPlayerCount() >= GetRequiredReadyCount();
}

void ATRGameState::SetMatchStarted(bool bInStarted)
{
	if (!ensureMsgf(HasAuthority(), TEXT("판 시작 상태는 서버에서만 바꿀 수 있습니다.")))
	{
		return;
	}

	bMatchStarted = bInStarted;
}

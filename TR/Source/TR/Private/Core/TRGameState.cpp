// TRGameState.cpp


#include "Core/TRGameState.h"
#include "TRTypes.h"
#include "Engine/DataTable.h"
#include "Interaction/TRActionComponent.h"
#include "Net/UnrealNetwork.h"
#include "Core/TRPlayerState.h"
#include "Mass/Combat/TRMassCombatSubsystem.h"

void ATRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRGameState, MonsterResource);
	DOREPLIFETIME(ATRGameState, ArmorLevel);
	DOREPLIFETIME(ATRGameState, TurretLevel);
	DOREPLIFETIME(ATRGameState, BuildingComponent);
	DOREPLIFETIME(ATRGameState, bMatchStarted);
}

void ATRGameState::BeginPlay()
{
	Super::BeginPlay();
	
	// 처치 보상은 서버에서만 누적한다. MonsterResource 는 복제되므로 클라이언트는 값만 받는다.
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UTRMassCombatSubsystem* Combat = World->GetSubsystem<UTRMassCombatSubsystem>();
	if (!IsValid(Combat))
	{
		return;
	}

	Combat->OnEnemyKilled.AddDynamic(this, &ATRGameState::HandleEnemyKilled);
}

void ATRGameState::HandleEnemyKilled(const FVector& Location, int32 ResourceValue)
{
	AddMonsterResource(ResourceValue);
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

void ATRGameState::AddMonsterResource(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0)
	{
		return;
	}
	MonsterResource += Amount;
}

const FTRArmorLevelRow* ATRGameState::GetArmorLevelRow(const TCHAR* ContextName) const
{
	if (!ensureMsgf(ArmorLevelTable, TEXT("%s: ArmorLevelTable not assigned"), *GetName()))
	{
		return nullptr;
	}

	const FTRArmorLevelRow* Row = ArmorLevelTable->FindRow<FTRArmorLevelRow>(FName(*FString::FromInt(ArmorLevel)), ContextName);
	ensureMsgf(Row, TEXT("%s: armor level row %d not found in ArmorLevelTable"), *GetName(), ArmorLevel);
	return Row;
}

const FTRTurretLevelRow* ATRGameState::GetTurretLevelRow(const TCHAR* ContextName) const
{
	if (!ensureMsgf(TurretLevelTable, TEXT("%s: TurretLevelTable not assigned"), *GetName()))
	{
		return nullptr;
	}

	const FTRTurretLevelRow* Row = TurretLevelTable->FindRow<FTRTurretLevelRow>(FName(*FString::FromInt(TurretLevel)), ContextName);
	ensureMsgf(Row, TEXT("%s: turret level row %d not found in TurretLevelTable"), *GetName(), TurretLevel);
	return Row;
}

int32 ATRGameState::GetArmorUpgradeCost() const
{
	// 최대 레벨이면 다음 레벨이 없다. 행 조회 자체를 하지 않는다.
	if (ArmorLevel >= MaxArmorLevel)
	{
		return INDEX_NONE;
	}

	const FTRArmorLevelRow* Row = GetArmorLevelRow(ANSI_TO_TCHAR(__FUNCTION__));
	return Row ? Row->UpgradeCost : INDEX_NONE;
}

int32 ATRGameState::GetTurretUpgradeCost() const
{
	if (TurretLevel >= MaxTurretLevel)
	{
		return INDEX_NONE;
	}

	const FTRTurretLevelRow* Row = GetTurretLevelRow(ANSI_TO_TCHAR(__FUNCTION__));
	return Row ? Row->UpgradeCost : INDEX_NONE;
}

bool ATRGameState::TryUpgradeArmorLevel()
{
	if (!HasAuthority())
	{
		return false;
	}

	// 최대 레벨이면 INDEX_NONE 이 나오므로 레벨 검사가 여기에 함께 걸린다.
	const int32 Cost = GetArmorUpgradeCost();
	if (Cost < 0)
	{
		return false;
	}

	// 올리기 전 최대 HP. 파츠가 현재 HP 비율을 유지하는 데 쓴다.
	const FTRArmorLevelRow* PrevRow = GetArmorLevelRow(ANSI_TO_TCHAR(__FUNCTION__));
	const float PrevMaxHP = PrevRow ? PrevRow->MaxHP : 0.f;

	if (!TryConsumeMonsterResource(Cost))
	{
		return false;
	}

	++ArmorLevel;
	OnArmorLevelUpgraded.Broadcast(PrevMaxHP);
	return true;
}

bool ATRGameState::TryUpgradeTurretLevel()
{
	if (!HasAuthority())
	{
		return false;
	}

	const int32 Cost = GetTurretUpgradeCost();
	if (Cost < 0)
	{
		return false;
	}

	const FTRTurretLevelRow* PrevRow = GetTurretLevelRow(ANSI_TO_TCHAR(__FUNCTION__));
	const float PrevMaxHP = PrevRow ? PrevRow->MaxHP : 0.f;

	if (!TryConsumeMonsterResource(Cost))
	{
		return false;
	}

	++TurretLevel;
	OnTurretLevelUpgraded.Broadcast(PrevMaxHP);
	return true;
}

int32 ATRGameState::GetReadyPlayerCount() const
{
	// 호스트는 시작 버튼을 쥐고 있으므로 항상 준비된 것으로 센다.
	int32 Count = 0;
	for (const APlayerState* State : PlayerArray)
	{
		const ATRPlayerState* TRState = Cast<ATRPlayerState>(State);
		if (TRState != nullptr && (TRState->IsHost() || TRState->IsReady()))
		{
			++Count;
		}
	}

	return Count;
}

int32 ATRGameState::GetRequiredReadyCount() const
{
	return PlayerArray.Num();
}

bool ATRGameState::CanStartMatch() const
{
	// 혼자면 호스트 하나로 1/1 이라 곧바로 시작할 수 있다.
	return !bMatchStarted && GetReadyPlayerCount() >= GetRequiredReadyCount();
}

void ATRGameState::SetMatchStarted(bool bInStarted)
{
	if (!ensureMsgf(HasAuthority(), TEXT("판 시작 상태는 서버에서만 바꿀 수 있습니다.")))
	{
		return;
	}

	// 같은 값을 다시 넣어도 통지가 두 번 나가지 않게 막는다.
	if (bMatchStarted == bInStarted)
	{
		return;
	}

	bMatchStarted = bInStarted;

	// 서버에는 OnRep 이 오지 않으므로 여기서 직접 알린다.
	OnMatchStartedChanged.Broadcast(bMatchStarted);
}

void ATRGameState::OnRep_MatchStarted()
{
	OnMatchStartedChanged.Broadcast(bMatchStarted);
}

void ATRGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 월드 종료 순서에 따라 서브시스템이 먼저 정리됐을 수 있으므로 확인하고 해제한다.
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		if (UTRMassCombatSubsystem* Combat = World->GetSubsystem<UTRMassCombatSubsystem>())
		{
			Combat->OnEnemyKilled.RemoveDynamic(this, &ATRGameState::HandleEnemyKilled);
		}
	}

	Super::EndPlay(EndPlayReason);
}

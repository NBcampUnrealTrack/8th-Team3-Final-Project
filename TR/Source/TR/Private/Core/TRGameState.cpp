// TRGameState.cpp


#include "Core/TRGameState.h"
#include "TRTypes.h"
#include "Debug/TRGameplayLog.h"
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
	DOREPLIFETIME(ATRGameState, WaveNumber);
	DOREPLIFETIME(ATRGameState, WavePhase);
	DOREPLIFETIME(ATRGameState, RestEndServerTime);
	DOREPLIFETIME(ATRGameState, AliveEnemyCount);
	DOREPLIFETIME(ATRGameState, Biome);
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

void ATRGameState::SetWaveNumber(int32 InWaveNumber)
{
	if (!ensureMsgf(HasAuthority(), TEXT("웨이브 번호는 서버에서만 바꿀 수 있습니다.")))
	{
		return;
	}

	if (WaveNumber == InWaveNumber)
	{
		return;
	}

	WaveNumber = InWaveNumber;

	// 서버에서는 OnRep 이 발화하지 않는다. 리슨 서버의 호스트도 같은 알림을 받아야
	// UI 가 클라이언트와 같은 시점에 갱신된다.
	OnRep_WaveNumber();
}

void ATRGameState::SetWavePhase(ETRWavePhase InPhase, float InRestEndServerTime)
{
	if (!ensureMsgf(HasAuthority(), TEXT("웨이브 단계는 서버에서만 바꿀 수 있습니다.")))
	{
		return;
	}

	// Over 는 종결 상태다. 판이 끝난 뒤에도 정리 과정에서 단계를 바꾸려는 호출이
	// 이어지는데(스포너의 StopWaves 등), 그걸 그대로 받으면 결과 화면 신호가 사라진다.
	// 여기서 막아 두면 호출하는 쪽이 순서를 신경 쓰지 않아도 된다.
	// 다시 판을 하려면 레벨을 새로 열어 GameState 를 새로 만든다.
	if (WavePhase == ETRWavePhase::Over && InPhase != ETRWavePhase::Over)
	{
		return;
	}

	// 끝나는 시각은 단계가 그대로여도 갱신될 수 있으므로(휴식 연장 등) 먼저 반영한다.
	RestEndServerTime = (InPhase == ETRWavePhase::Rest) ? InRestEndServerTime : 0.f;

	if (WavePhase == InPhase)
	{
		return;
	}

	WavePhase = InPhase;
	OnRep_WavePhase();
}

void ATRGameState::SetBiome(const ETRBiomeType InBiome)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Biome == InBiome)
	{
		return;
	}

	// 이 값은 OnRep 이 없어서(단순 복제) 클라이언트 쪽 시점은 여기에 안 남는다.
	// 클라이언트가 실제로 언제 받았는지는 ATRLevelSceneManager 의 풍경 전환 로그를 볼 것.
	UE_LOG(LogTRGameplay, Log, TEXT("[Wave] 서버 웨이브 %d 의 풍경을 %s 로 지정"),
		WaveNumber, *UEnum::GetValueAsString(InBiome));

	Biome = InBiome;
}

void ATRGameState::SetAliveEnemyCount(int32 InCount)
{
	if (!HasAuthority())
	{
		return;
	}

	// 값이 그대로면 복제 대상에서 빠지므로 여기서 따로 거를 것은 없지만,
	// 음수는 UI 가 그대로 찍으므로 여기서 자른다.
	AliveEnemyCount = FMath::Max(0, InCount);
}

float ATRGameState::GetRestTimeRemaining() const
{
	if (WavePhase != ETRWavePhase::Rest)
	{
		return 0.f;
	}

	// GetServerWorldTimeSeconds 는 클라이언트에서 서버 시간으로 보정된 값을 준다.
	// 반환형이 double 이라 계산도 double 로 하고 마지막에 한 번만 줄인다.
	const double Remaining = static_cast<double>(RestEndServerTime) - GetServerWorldTimeSeconds();
	return static_cast<float>(FMath::Max(0.0, Remaining));
}

void ATRGameState::OnRep_WaveNumber()
{
	// **여기가 서버와 클라이언트가 만나는 유일한 지점이다.** 서버는 SetWaveNumber 가
	// 직접 부르고 클라이언트는 복제가 부르므로, 이 한 곳에 찍으면 양쪽 로그를 나란히
	// 놓고 어긋난 시점을 찾을 수 있다. 세터에 찍으면 클라이언트 로그가 통째로 빈다.
	UE_LOG(LogTRGameplay, Log, TEXT("[Wave] %s 웨이브 %d"),
		HasAuthority() ? TEXT("서버") : TEXT("클라"), WaveNumber);

	OnWaveNumberChanged.Broadcast(WaveNumber);
}

void ATRGameState::OnRep_WavePhase()
{
	UE_LOG(LogTRGameplay, Log, TEXT("[Wave] %s 웨이브 %d 단계 -> %s"),
		HasAuthority() ? TEXT("서버") : TEXT("클라"), WaveNumber,
		*UEnum::GetValueAsString(WavePhase));

	OnWavePhaseChanged.Broadcast(WavePhase);
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

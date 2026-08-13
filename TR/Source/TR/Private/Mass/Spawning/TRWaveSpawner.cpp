// TRWaveSpawner.cpp

#include "Mass/Spawning/TRWaveSpawner.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Targeting/TRTargetDirectorSubsystem.h"
#include "TimerManager.h"
#include "Train/TRTrain.h"

ATRWaveSpawner::ATRWaveSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// 웨이브 진행은 서버 권위다. 클라이언트는 결과(복제된 표현)만 본다.
	bReplicates = false;
}

void ATRWaveSpawner::BeginPlay()
{
	Super::BeginPlay();

	// 게임모드가 있는 쪽(서버/스탠드얼론)에서만 진행한다.
	if (!HasAuthority())
	{
		return;
	}

	if (TargetActor == nullptr)
	{
		// 레벨에 기차가 하나뿐인 구성을 전제로 한다. 여러 대가 되면 명시 지정이 필요하다.
		for (TActorIterator<ATRTrain> It(GetWorld()); It; ++It)
		{
			TargetActor = *It;
			break;
		}
	}

	if (TargetActor == nullptr)
	{
		UE_LOG(LogTRMass, Warning, TEXT("웨이브 스포너: 대상 액터를 찾지 못했습니다. 스폰 중심이 원점이 됩니다."));
	}
	else if (bAutoRegisterTarget)
	{
		// 추적 목표도 여기서 함께 등록한다.
		// 이게 없으면 "누가 목표인가"를 블루프린트에서 한 번 더 지정해야 하고,
		// 둘이 어긋나면 적이 엉뚱한 곳으로 가거나 아예 안 움직인다.
		if (UTRTargetDirectorSubsystem* Director = GetWorld()->GetSubsystem<UTRTargetDirectorSubsystem>())
		{
			// 추적 영역은 이 시점에 대상 액터에서 잰다. 기차의 TargetVolume 컴포넌트가
			// 있으면 그것을, 없으면 메시 전체 바운드를 쓴다.
			Director->SetTargetActor(TargetChannel, TargetActor);

			UE_LOG(LogTRMass, Log, TEXT("웨이브 스포너: 채널[%d] 목표를 '%s'로 등록했습니다."),
				static_cast<int32>(TargetChannel), *TargetActor->GetName());
		}
	}

	// 적의 폭발 피해가 갈 곳도 위에서 등록한 그 목표다. 따로 걸어줄 것이 없다 —
	// 전투 서브시스템이 채널의 목표를 그대로 쓴다.

	if (UTRWaveDirectorSubsystem* Director = GetWorld()->GetSubsystem<UTRWaveDirectorSubsystem>())
	{
		Director->OnWaveFinished.AddDynamic(this, &ATRWaveSpawner::HandleWaveFinished);
	}

	if (bAutoStartOnBeginPlay)
	{
		StartWaves();
	}
}

void ATRWaveSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopWaves();

	if (UWorld* World = GetWorld())
	{
		if (UTRWaveDirectorSubsystem* Director = World->GetSubsystem<UTRWaveDirectorSubsystem>())
		{
			Director->OnWaveFinished.RemoveDynamic(this, &ATRWaveSpawner::HandleWaveFinished);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ATRWaveSpawner::StartWaves()
{
	bRunning = true;
	StartWaveNumber(FirstWaveNumber);
}

void ATRWaveSpawner::StopWaves()
{
	bRunning = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestTimerHandle);

		if (UTRWaveDirectorSubsystem* Director = World->GetSubsystem<UTRWaveDirectorSubsystem>())
		{
			Director->StopWave();
		}
	}
}

void ATRWaveSpawner::StartWaveNumber(int32 WaveNumber)
{
	UWorld* World = GetWorld();
	UTRWaveDirectorSubsystem* Director = World ? World->GetSubsystem<UTRWaveDirectorSubsystem>() : nullptr;
	if (Director == nullptr)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(RestTimerHandle);

	int32 SourceWaveNumber = WaveNumber;
	const FTRWaveRow* Row = FindWaveRow(WaveNumber, SourceWaveNumber);
	if (Row == nullptr)
	{
		UE_LOG(LogTRMass, Warning, TEXT("웨이브 %d: 테이블에서 행을 찾지 못했습니다. 진행을 멈춥니다."), WaveNumber);
		StopWaves();
		return;
	}

	CurrentWaveNumber = WaveNumber;
	bRunning = true;

	// 테이블에 그 웨이브가 없으면 마지막 행을 복리로 키워서 쓴다.
	// 이렇게 하면 테이블에 웨이브를 무한히 나열하지 않아도 된다.
	FTRWaveDefinition Definition = Row->Wave;
	if (SourceWaveNumber != WaveNumber)
	{
		Definition = UTRWaveDirectorSubsystem::ScaleWaveForNumber(
			Row->Wave, WaveNumber, SourceWaveNumber, ScalingStartWave, PerWaveMultiplier);

		UE_LOG(LogTRMass, Log, TEXT("웨이브 %d: 테이블에 없어 %d행을 스케일링 (그룹 %d개, 스탯 ×%.2f)"),
			WaveNumber, SourceWaveNumber, Definition.Groups.Num(), Definition.StatScale);
	}

	// 이 웨이브가 쓰는 채널에 목표가 없으면 적이 스폰만 되고 움직이지 않는다.
	// 에러 없이 조용히 실패하는 지점이라 여기서 먼저 잡는다.
	if (const UTRTargetDirectorSubsystem* TargetDirector = World->GetSubsystem<UTRTargetDirectorSubsystem>())
	{
		if (!TargetDirector->HasTarget(Definition.Channel))
		{
			UE_LOG(LogTRMass, Error,
				TEXT("웨이브 %d: 채널[%d]에 목표가 없습니다. 적이 스폰되어도 추적하지 않습니다. ")
				TEXT("웨이브 테이블의 Channel과 스포너의 Target Channel이 같은지 확인하세요."),
				WaveNumber, static_cast<int32>(Definition.Channel));
		}
	}

	UE_LOG(LogTRMass, Log, TEXT("웨이브 %d 시작"), WaveNumber);

	OnWaveStarted.Broadcast(WaveNumber);
	Director->StartWave(Definition, TargetActor);
}

bool ATRWaveSpawner::IsResting() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(RestTimerHandle);
}

const FTRWaveRow* ATRWaveSpawner::FindWaveRow(int32 WaveNumber, int32& OutSourceWaveNumber) const
{
	if (WaveTable == nullptr)
	{
		return nullptr;
	}

	static const TCHAR* ContextString = TEXT("ATRWaveSpawner::FindWaveRow");

	// 정확히 일치하는 행이 있으면 그대로 쓴다.
	OutSourceWaveNumber = WaveNumber;
	if (const FTRWaveRow* ExactRow = WaveTable->FindRow<FTRWaveRow>(FName(*FString::FromInt(WaveNumber)), ContextString, /*bWarnIfRowMissing=*/false))
	{
		return ExactRow;
	}

	// 없으면 그보다 작은 번호 중 가장 큰 행을 찾는다.
	const FTRWaveRow* BestRow = nullptr;
	int32 BestNumber = INDEX_NONE;

	for (const TPair<FName, uint8*>& Pair : WaveTable->GetRowMap())
	{
		const FString RowName = Pair.Key.ToString();
		const int32 RowNumber = FCString::Atoi(*RowName);

		// 행 이름이 곧 웨이브 번호다. 숫자가 아니면 어느 웨이브인지 알 수 없어 쓸 수 없다.
		if (RowNumber <= 0)
		{
			UE_LOG(LogTRMass, Warning,
				TEXT("웨이브 테이블 '%s': 행 이름 '%s'을(를) 웨이브 번호로 읽을 수 없어 건너뜁니다. ")
				TEXT("행 이름은 '1', '2' 처럼 웨이브 번호만 적으세요."),
				*GetNameSafe(WaveTable), *RowName);
			continue;
		}

		if (RowNumber > WaveNumber || RowNumber <= BestNumber)
		{
			continue;
		}

		BestNumber = RowNumber;
		BestRow = reinterpret_cast<const FTRWaveRow*>(Pair.Value);
	}

	OutSourceWaveNumber = BestNumber;
	return BestRow;
}

void ATRWaveSpawner::HandleWaveFinished()
{
	if (!bRunning)
	{
		return;
	}

	OnWaveCleared.Broadcast(CurrentWaveNumber);

	// 방금 끝난 웨이브의 휴식 시간을 쓴다.
	int32 SourceWaveNumber = CurrentWaveNumber;
	const FTRWaveRow* Row = FindWaveRow(CurrentWaveNumber, SourceWaveNumber);
	const float RestDuration = Row ? Row->RestDuration : 0.f;

	UE_LOG(LogTRMass, Log, TEXT("웨이브 %d 배치 종료. 휴식 %.1f초"), CurrentWaveNumber, RestDuration);

	if (RestDuration > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(RestTimerHandle, this,
			&ATRWaveSpawner::AdvanceToNextWave, RestDuration, /*bLoop=*/false);
	}
	else
	{
		AdvanceToNextWave();
	}
}

void ATRWaveSpawner::AdvanceToNextWave()
{
	if (!bRunning)
	{
		return;
	}

	StartWaveNumber(CurrentWaveNumber + 1);
}

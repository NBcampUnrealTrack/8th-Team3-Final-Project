// TRWaveSpawner.cpp

#include "Mass/Spawning/TRWaveSpawner.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Core/TRGameState.h"
#include "Mass/Core/TRMassParams.h"
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

	// 여기까지(목표·표적 구역 등록)는 **클라이언트도 해야 한다.** 복제로 받은 적의
	// 표현이 기차를 기준으로 놓이고, 디버그 표시도 양쪽에서 같은 기준을 봐야 한다.
	// 웨이브 진행(스폰 판단)만 서버 권위다 — 그건 아래에서 가른다.
	if (!HasAuthority())
	{
		return;
	}

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

	// 생존 수 갱신은 웨이브가 도는 동안에만 필요하다. 판 시작 전이나 종료 뒤에
	// 계속 돌면 아무도 안 보는 값을 위해 핸들 배열만 훑게 된다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AliveCountTimerHandle, this,
			&ATRWaveSpawner::PushAliveCount, FMath::Max(0.1f, AliveCountPushInterval), /*bLoop=*/true);
	}

	StartWaveNumber(FirstWaveNumber);
}

void ATRWaveSpawner::StopWaves()
{
	bRunning = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestTimerHandle);
		World->GetTimerManager().ClearTimer(AliveCountTimerHandle);

		if (UTRWaveDirectorSubsystem* Director = World->GetSubsystem<UTRWaveDirectorSubsystem>())
		{
			Director->StopWave();
		}
	}

	// 진행이 멈췄다는 사실을 클라이언트도 알아야 한다. 안 그러면 HUD 가 마지막
	// 웨이브 화면이나 다 끝난 휴식 카운트다운(0초)에 그대로 멈춰 있는다.
	//
	// 판 종료로 불려 온 경우에는 이 호출이 무시된다 — Over 는 종결 상태라
	// ATRGameState::SetWavePhase 가 벗어나는 전이를 받지 않는다.
	PushWavePhase(ETRWavePhase::None);
}

bool ATRWaveSpawner::SkipRest()
{
	if (!bRunning || !IsResting())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	// 남은 시간은 타이머를 지우기 전에 읽는다.
	UE_LOG(LogTRMass, Log, TEXT("휴식을 건너뜁니다 (남은 %.1f초)."), GetRestTimeRemaining());

	World->GetTimerManager().ClearTimer(RestTimerHandle);

	AdvanceToNextWave();
	return true;
}

ATRGameState* ATRWaveSpawner::GetTRGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ATRGameState>() : nullptr;
}

void ATRWaveSpawner::PushWavePhase(ETRWavePhase Phase, float RestDuration)
{
	ATRGameState* GameState = GetTRGameState();
	if (GameState == nullptr || !HasAuthority())
	{
		return;
	}

	// 남은 시간이 아니라 **끝나는 시각**을 넘긴다. 남은 시간을 복제하면 복제 주기
	// 사이에 클라이언트의 카운트다운이 계단처럼 멈춰 보인다.
	const float RestEndServerTime = (Phase == ETRWavePhase::Rest)
		? GetWorld()->GetTimeSeconds() + RestDuration
		: 0.f;

	GameState->SetWavePhase(Phase, RestEndServerTime);
}

void ATRWaveSpawner::PushBiomeForWave(const int32 WaveNumber)
{
	ATRGameState* GameState = GetTRGameState();
	if (GameState == nullptr || !HasAuthority())
	{
		return;
	}

	int32 SourceWaveNumber = 0;
	const FTRWaveRow* Row = FindWaveRow(WaveNumber, SourceWaveNumber);
	if (Row == nullptr || Row->Biome == ETRBiomeType::None)
	{
		// None 은 "직전 풍경 유지"다. 지우는 뜻이 아니므로 덮어쓰지 않는다.
		return;
	}

	if (Row->Biome == GameState->GetBiome())
	{
		return;
	}

	GameState->SetBiome(Row->Biome);

	UE_LOG(LogTRMass, Log, TEXT("웨이브 %d 풍경: %s"), WaveNumber, *UEnum::GetValueAsString(Row->Biome));
}

void ATRWaveSpawner::PushAliveCount()
{
	ATRGameState* GameState = GetTRGameState();
	if (GameState == nullptr)
	{
		return;
	}

	const UWorld* World = GetWorld();
	UTRWaveDirectorSubsystem* Director = World ? World->GetSubsystem<UTRWaveDirectorSubsystem>() : nullptr;

	// 디렉터가 없으면 셀 방법이 없다. 0으로 덮으면 UI 가 "전멸"로 오해하므로 그대로 둔다.
	if (Director == nullptr)
	{
		return;
	}

	GameState->SetAliveEnemyCount(Director->GetAliveCount());
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

	// 휴식 중이었는지는 단계를 덮기 **전에** 봐야 한다. 타이머만으로는 판정할 수 없다 —
	// 정상 진행이면 이 시점엔 이미 타이머가 발화해 사라진 뒤다.
	ATRGameState* GameState = GetTRGameState();
	const bool bWasResting = (GameState != nullptr) && GameState->IsResting();

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

	// 스폰 규칙과 휴식 시간을 같은 행에서 함께 읽어 둔다. 휴식은 이 웨이브가
	// 끝난 뒤에 쓰지만, 그때 다시 찾으면 시작 때와 다른 행을 볼 수 있다.
	CurrentRestDuration = Row->RestDuration;

	// 휴식을 빠져나오는 모든 경로(타이머 만료, SkipRest, 테스트용 점프)가 여기를 지난다.
	if (bWasResting)
	{
		OnRestEnded.Broadcast();
	}

	const FTRWaveDefinition Definition = ResolveWaveDefinition(*Row, WaveNumber, SourceWaveNumber);

	LogWaveStatScales(Definition, WaveNumber);
	VerifyWaveTarget(Definition, WaveNumber);

	UE_LOG(LogTRMass, Log, TEXT("웨이브 %d 시작"), WaveNumber);

	// 클라이언트로 나가는 값은 여기서 갱신한다. 스폰을 시작하기 **전**이라,
	// 첫 배치가 나오는 순간에는 이미 HUD 가 새 웨이브 번호를 들고 있다.
	if (GameState != nullptr)
	{
		GameState->SetWaveNumber(WaveNumber);
	}
	PushWavePhase(ETRWavePhase::Combat);

	// 판이 막 시작됐거나 휴식을 건너뛴 경우를 위해 여기서도 한 번 맞춘다.
	// 정상 흐름에서는 휴식에 들어갈 때 이미 밀어 넣었으므로 같은 값이 다시 들어간다.
	PushBiomeForWave(WaveNumber);

	OnWaveStarted.Broadcast(WaveNumber);
	Director->StartWave(Definition, TargetActor);
}

FTRWaveDefinition ATRWaveSpawner::ResolveWaveDefinition(const FTRWaveRow& Row, int32 WaveNumber, int32 SourceWaveNumber) const
{
	// 행 번호가 웨이브 번호와 같으면 테이블에 그 웨이브가 있다는 뜻이다. 그대로 쓴다.
	if (SourceWaveNumber == WaveNumber)
	{
		return Row.Wave;
	}

	// 없으면 마지막 행을 복리로 키워서 쓴다. 덕분에 테이블에 웨이브를 무한히 나열하지 않아도 된다.
	FTRWaveDefinition Scaled = UTRWaveDirectorSubsystem::ScaleWaveForNumber(
		Row.Wave, WaveNumber, SourceWaveNumber, ScalingStartWave, PerWaveMultiplier);

	UE_LOG(LogTRMass, Log, TEXT("웨이브 %d: 테이블에 없어 %d행을 스케일링 (그룹 %d개)"),
		WaveNumber, SourceWaveNumber, Scaled.Groups.Num());

	return Scaled;
}

void ATRWaveSpawner::LogWaveStatScales(const FTRWaveDefinition& Definition, int32 WaveNumber) const
{
	UE_LOG(LogTRMass, Log, TEXT("웨이브 %d 배수(웨이브 축): 체력 ×%.2f, 피해 ×%.2f, 속도 ×%.2f, 자원 +%d"),
		WaveNumber, Definition.TierStatScale.Health, Definition.TierStatScale.Damage,
		Definition.TierStatScale.Speed, Definition.TierStatScale.ResourceBonus);

	// 종류 축 배수는 그룹마다 다르므로 1이 아닌 것만 따로 찍는다. 로그에 안 보이면
	// 테이블에 적어 둔 종류별 차등이 왜 안 먹는지 추적할 방법이 없다.
	for (int32 GroupIndex = 0; GroupIndex < Definition.Groups.Num(); ++GroupIndex)
	{
		const FTREnemyStatScale& TypeScale = Definition.Groups[GroupIndex].TypeStatScale;

		const bool bIsIdentity = FMath::IsNearlyEqual(TypeScale.Health, 1.f)
			&& FMath::IsNearlyEqual(TypeScale.Damage, 1.f)
			&& FMath::IsNearlyEqual(TypeScale.Speed, 1.f)
			&& (TypeScale.ResourceBonus == 0);

		if (!bIsIdentity)
		{
			UE_LOG(LogTRMass, Log, TEXT("  그룹 %d(%s) 배수(종류 축): 체력 ×%.2f, 피해 ×%.2f, 속도 ×%.2f, 자원 +%d"),
				GroupIndex, LexToString(Definition.Groups[GroupIndex].EnemyType),
				TypeScale.Health, TypeScale.Damage, TypeScale.Speed, TypeScale.ResourceBonus);
		}
	}
}

void ATRWaveSpawner::VerifyWaveTarget(const FTRWaveDefinition& Definition, int32 WaveNumber) const
{
	const UWorld* World = GetWorld();
	const UTRTargetDirectorSubsystem* TargetDirector =
		World ? World->GetSubsystem<UTRTargetDirectorSubsystem>() : nullptr;

	if (TargetDirector == nullptr || TargetDirector->HasTarget(Definition.Channel))
	{
		return;
	}

	// 채널에 목표가 없으면 적이 스폰만 되고 움직이지 않는다.
	// 에러 없이 조용히 실패하는 지점이라 여기서 먼저 잡는다.
	UE_LOG(LogTRMass, Error,
		TEXT("웨이브 %d: 채널[%d]에 목표가 없습니다. 적이 스폰되어도 추적하지 않습니다. ")
		TEXT("웨이브 테이블의 Channel과 스포너의 Target Channel이 같은지 확인하세요."),
		WaveNumber, static_cast<int32>(Definition.Channel));
}

bool ATRWaveSpawner::IsResting() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(RestTimerHandle);
}

float ATRWaveSpawner::GetRestTimeRemaining() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 0.f;
	}

	// 타이머가 없으면 -1 이 나온다. 휴식이 아니라는 뜻이므로 0 으로 정리해서 준다.
	return FMath::Max(0.f, World->GetTimerManager().GetTimerRemaining(RestTimerHandle));
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

	// 방금 끝난 웨이브의 휴식 시간. 시작할 때 그 행에서 읽어 둔 값이다.
	const float RestDuration = CurrentRestDuration;

	UE_LOG(LogTRMass, Log, TEXT("웨이브 %d 배치 종료. 휴식 %.1f초"), CurrentWaveNumber, RestDuration);

	if (RestDuration > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(RestTimerHandle, this,
			&ATRWaveSpawner::AdvanceToNextWave, RestDuration, /*bLoop=*/false);

		// **다음** 웨이브의 바이옴을 지금 알린다. 배경은 휴식 동안 흘러 들어와야
		// 전투가 시작될 때 이미 바뀌어 있다. 전투가 시작된 뒤에 바꾸면 싸우는 도중에
		// 풍경이 흘러와 산만하고, 그때는 볼 여유도 없다.
		PushBiomeForWave(CurrentWaveNumber + 1);

		// 타이머를 건 **뒤에** 알린다. 받은 쪽이 곧바로 GetRestTimeRemaining 을
		// 물어봐도 값이 나오도록.
		PushWavePhase(ETRWavePhase::Rest, RestDuration);
		OnRestStarted.Broadcast(RestDuration);
	}
	else
	{
		// 휴식이 0초면 Rest 단계를 거치지 않는다. 한 프레임짜리 단계를 만들면
		// UI 가 휴식 화면을 띄웠다 지우며 깜빡인다.
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

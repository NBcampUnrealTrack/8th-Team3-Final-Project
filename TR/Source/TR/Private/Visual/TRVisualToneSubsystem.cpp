// TRVisualToneSubsystem.cpp

#include "Visual/TRVisualToneSubsystem.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Core/TRGameState.h"
#include "Environment/TRGroundMotionSubsystem.h"
#include "Debug/TRGameplayLog.h"
#include "Train/TRTrain.h"

void UTRVisualToneSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 값들의 초기 상태는 멤버 선언의 기본값이 그대로 맞다(로비 = 평온).
	// 여기서 다시 대입하지 않는 이유는 두 곳에서 초기값을 정하면 한쪽만 고쳤을 때
	// 어긋나기 때문이다. LastPushed* 가 -1 로 시작하는 것도 같은 곳에 적혀 있으며,
	// 그 덕분에 첫 Tick 의 PushToCollection 이 반드시 한 번은 MPC 에 쓴다
	// (안 그러면 에디터에서 실험하다 애셋에 저장된 값이 판에 그대로 나온다).
}

void UTRVisualToneSubsystem::Deinitialize()
{
	if (ATRGameState* GameState = CachedGameState.Get())
	{
		GameState->OnWaveNumberChanged.RemoveAll(this);
		GameState->OnWavePhaseChanged.RemoveAll(this);
	}

	CachedGameState.Reset();
	CachedTrain.Reset();
	CachedInstance.Reset();

	Super::Deinitialize();
}

bool UTRVisualToneSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 에디터 프리뷰 월드까지 만들면 톤 값이 여러 월드에서 같은 MPC 를 두고 다툰다.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UTRVisualToneSubsystem::IsTickable() const
{
	if (!Super::IsTickable())
	{
		return false;
	}

	// 데디케이티드 서버는 아무것도 그리지 않는다. 머테리얼 파라미터를 굴릴 이유가 없다.
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() != NM_DedicatedServer;
}

TStatId UTRVisualToneSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTRVisualToneSubsystem, STATGROUP_Tickables);
}

bool UTRVisualToneSubsystem::EnsureGameStateBound()
{
	if (CachedGameState.IsValid())
	{
		return true;
	}

	UWorld* World = GetWorld();
	ATRGameState* GameState = World ? World->GetGameState<ATRGameState>() : nullptr;
	if (GameState == nullptr)
	{
		// 클라이언트에서는 접속 직후 몇 프레임 동안 정상적으로 비어 있다. 로그를 남기지 않는다.
		return false;
	}

	CachedGameState = GameState;

	GameState->OnWaveNumberChanged.AddDynamic(this, &UTRVisualToneSubsystem::HandleWaveNumberChanged);
	GameState->OnWavePhaseChanged.AddDynamic(this, &UTRVisualToneSubsystem::HandleWavePhaseChanged);

	// 델리게이트는 앞으로의 변화만 알려준다. 붙기 전에 이미 전투가 시작됐다면
	// (늦게 접속했거나 재접속) 지금 값으로 한 번 맞춰 줘야 한다.
	HandleWavePhaseChanged(GameState->GetWavePhase());

	return true;
}

float UTRVisualToneSubsystem::ResolveHullRatio()
{
	ATRTrain* Train = CachedTrain.Get();
	if (Train == nullptr)
	{
		UWorld* World = GetWorld();
		if (World == nullptr)
		{
			return 1.f;
		}

		// **찾기를 초당 한 번으로 묶는다.**
		//
		// 기차가 없는 레벨(메인 메뉴 등)에서는 캐시가 영원히 안 채워지므로, 조건 없이
		// 두면 매 프레임 월드의 액터를 통째로 순회한다. 화면에 바뀌는 것도 없는
		// 레벨에서 조용히 프레임을 먹는 종류라 눈에 띄지도 않는다.
		const float Now = World->GetTimeSeconds();
		if (Now < NextTrainSearchTime)
		{
			return 1.f;
		}
		NextTrainSearchTime = Now + TrainSearchInterval;

		// 레벨에 기차는 하나다. 찾으면 캐시하므로 순회는 붙을 때까지만 돈다.
		for (TActorIterator<ATRTrain> It(World); It; ++It)
		{
			Train = *It;
			CachedTrain = Train;
			break;
		}

		if (Train == nullptr)
		{
			return 1.f;
		}
	}

	const float MaxHP = Train->GetMaxHullHealth();
	if (MaxHP <= 0.f)
	{
		return 1.f;
	}

	return FMath::Clamp(Train->GetCurrentHullHealth() / MaxHP, 0.f, 1.f);
}

void UTRVisualToneSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UTRVisualToneSettings* Settings = GetDefault<UTRVisualToneSettings>();

	// 게임 상태가 아직 없어도 계속 진행한다. 선체 비율과 감쇠는 그와 무관하게 굴러야
	// 하고, 연결은 다음 틱에 다시 시도된다.
	EnsureGameStateBound();

	// ── 긴장도 ───────────────────────────────────────────────────────────────
	if (Settings->TensionTransitionTime > KINDA_SMALL_NUMBER)
	{
		// 상수 속도 보간을 쓴다. FInterpTo 는 목표에 가까울수록 느려져서 "몇 초 만에
		// 바뀐다"는 기획 수치가 화면에서 지켜지지 않는다.
		Tension = FMath::FInterpConstantTo(Tension, TargetTension, DeltaTime, 1.f / Settings->TensionTransitionTime);
	}
	else
	{
		Tension = TargetTension;
	}

	// ── 선체 위험도 ──────────────────────────────────────────────────────────
	//
	// 문턱 아래로 내려간 만큼만 0~1 로 편다. 문턱 위에서는 0이라 평소 화면이 깨끗하다.
	const float HullRatio = ResolveHullRatio();
	const float Threshold = FMath::Max(Settings->HullDangerThreshold, KINDA_SMALL_NUMBER);
	HullDanger = FMath::Clamp((Threshold - HullRatio) / Threshold, 0.f, 1.f);

	// ── 지면 속도 ────────────────────────────────────────────────────────────
	//
	// 지형이 흐르는 속도가 곧 기차의 속도다. 기차 액터는 월드에서 멈춰 있으므로
	// 액터의 속도를 재면 언제나 0이다(트레드밀 좌표계).
	{
		const UTRGroundMotionSubsystem* GroundMotion = GetWorld()
			? GetWorld()->GetSubsystem<UTRGroundMotionSubsystem>()
			: nullptr;

		const float RawSpeed = (GroundMotion != nullptr)
			? static_cast<float>(GroundMotion->GetGroundVelocity().Size())
			: 0.f;

		const float TargetSpeed = FMath::Clamp(RawSpeed / FMath::Max(Settings->SpeedReference, 1.f), 0.f, 1.f);

		GroundSpeed = (Settings->SpeedTransitionTime > KINDA_SMALL_NUMBER)
			? FMath::FInterpConstantTo(GroundSpeed, TargetSpeed, DeltaTime, 1.f / Settings->SpeedTransitionTime)
			: TargetSpeed;
	}

	// ── 웨이브 섬광 ──────────────────────────────────────────────────────────
	if (Pulse > 0.f)
	{
		Pulse = (Settings->WavePulseDuration > KINDA_SMALL_NUMBER)
			? FMath::Max(0.f, Pulse - DeltaTime / Settings->WavePulseDuration)
			: 0.f;
	}

	PushToCollection();
}

void UTRVisualToneSubsystem::PushToCollection()
{
	// 셋 다 문턱 안이면 쓸 것이 없다. 0에 막 닿은 프레임은 예외로 한 번 써서
	// 값이 문턱만큼 뜬 채로 멈추지 않게 한다.
	const bool bTensionSettled = FMath::IsNearlyEqual(Tension, LastPushedTension, PushThreshold);
	const bool bDangerSettled = FMath::IsNearlyEqual(HullDanger, LastPushedDanger, PushThreshold);
	const bool bPulseSettled = FMath::IsNearlyEqual(Pulse, LastPushedPulse, PushThreshold)
		&& !(Pulse <= 0.f && LastPushedPulse > 0.f);
	const bool bSpeedSettled = FMath::IsNearlyEqual(GroundSpeed, LastPushedSpeed, PushThreshold);

	if (bTensionSettled && bDangerSettled && bPulseSettled && bSpeedSettled)
	{
		return;
	}

	UMaterialParameterCollectionInstance* Instance = CachedInstance.Get();

	if (Instance == nullptr)
	{
		// 소프트 참조를 푸는 것도, 월드에서 인스턴스를 찾는 것도 한 번이면 된다.
		const UTRVisualToneSettings* Settings = GetDefault<UTRVisualToneSettings>();
		UMaterialParameterCollection* Collection = Settings->ToneCollection.LoadSynchronous();

		UWorld* World = GetWorld();
		Instance = (Collection != nullptr && World != nullptr)
			? World->GetParameterCollectionInstance(Collection)
			: nullptr;

		CachedInstance = Instance;
	}

	if (Instance == nullptr)
	{
		if (!bWarnedMissingCollection)
		{
			bWarnedMissingCollection = true;
			UE_LOG(LogTRGameplay, Warning,
				TEXT("[VisualTone] 파라미터 컬렉션이 없다. 프로젝트 세팅 > Game > TR Visual Tone 에서 지정할 것. 톤 전환은 동작하지 않는다."));
		}
		return;
	}

	LastPushedTension = Tension;
	LastPushedDanger = HullDanger;
	LastPushedPulse = Pulse;
	LastPushedSpeed = GroundSpeed;

	// SetScalarParameterValue 는 이름이 컬렉션에 없으면 false 를 돌려주고 조용히 넘어간다.
	// 그 조용함이 이 기능이 실패하는 가장 흔한 방식이라, 처음 한 번은 짚어준다.
	const bool bWrote = Instance->SetScalarParameterValue(TRVisualToneParams::WaveTension, Tension)
		&& Instance->SetScalarParameterValue(TRVisualToneParams::HullDanger, HullDanger)
		&& Instance->SetScalarParameterValue(TRVisualToneParams::WavePulse, Pulse)
		&& Instance->SetScalarParameterValue(TRVisualToneParams::GroundSpeed, GroundSpeed);

	if (!bWrote && !bWarnedMissingCollection)
	{
		bWarnedMissingCollection = true;
		UE_LOG(LogTRGameplay, Warning,
			TEXT("[VisualTone] 컬렉션에 WaveTension/HullDanger/WavePulse/GroundSpeed 스칼라 파라미터가 없다. 이름이 정확히 일치해야 한다."));
	}
}

void UTRVisualToneSubsystem::HandleWaveNumberChanged(int32 WaveNumber)
{
	// 0은 "아직 시작 안 함"이다. 로비에서 화면을 번쩍이게 할 이유가 없다.
	if (WaveNumber > 0)
	{
		Pulse = 1.f;
	}
}

void UTRVisualToneSubsystem::HandleWavePhaseChanged(ETRWavePhase Phase)
{
	// 판이 끝난 화면은 전투도 휴식도 아니다. 긴장을 풀어 결과 UI 가 잘 읽히게 한다.
	TargetTension = (Phase == ETRWavePhase::Combat) ? 1.f : 0.f;
}

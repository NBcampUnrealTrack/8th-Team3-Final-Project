// TRMassNetRelay.cpp

#include "Mass/Net/TRMassNetRelay.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassEntityTemplate.h"
#include "MassEntityUtils.h"
#include "MassLODCollectorProcessor.h"
#include "MassMovementFragments.h"
#include "MassRepresentationProcessor.h"
#include "MassSpawnerSubsystem.h"
#include "MassVisualizationLODProcessor.h"
#include "Net/UnrealNetwork.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Debug/TRMassDebug.h"

// ── FTRMassAgentSnapshot — 굽기와 되돌리기 ──────────────────────────────────
//
// 두 함수는 서로의 거울이다. 한쪽에만 필드를 더하면 그 값은 클라이언트에서 조용히
// 기본값으로 남는다(헤더의 주석 참고). 고칠 때는 항상 둘을 같이 본다.

void FTRMassAgentSnapshot::CollectFrom(const FTransform& Transform, const FTRMassNetIdFragment& NetIdFragment,
	const FTREnemyStateFragment* State, const FTREnemyVisualFragment* Visual)
{
	NetId = NetIdFragment.NetId;
	ConfigIndex = NetIdFragment.ConfigIndex;
	Location = Transform.GetLocation();

	// Yaw 를 1바이트로. 음수 각도도 감싸도록 0~360 으로 정규화한 뒤 나눈다.
	const float YawDegrees = FRotator::ClampAxis(Transform.GetRotation().Rotator().Yaw);
	Yaw = static_cast<uint8>(FMath::RoundToInt(YawDegrees * (256.f / 360.f)));

	ActionState = (State != nullptr) ? State->ActionState : ETREnemyActionState::None;

	// 0~1 을 1바이트로. 256등분이면 손상 단계 하나가 수십 칸이라 눈에 띄는 계단이 없다.
	const float Damage = (Visual != nullptr) ? FMath::Clamp(Visual->DamageRatio, 0.f, 1.f) : 0.f;
	DamageRatio = static_cast<uint8>(FMath::RoundToInt(Damage * 255.f));
}

void FTRMassAgentSnapshot::ApplyTo(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
	float ClientWorldTime) const
{
	// 행동 상태를 그대로 옮긴다. 클라이언트의 애니메이션 선택이 이 값을 읽는다.
	if (FTREnemyStateFragment* State = EntityManager.GetFragmentDataPtr<FTREnemyStateFragment>(Entity))
	{
		// 진입 시각은 서버 시각이 아니라 **클라이언트가 그 상태를 처음 본 시각**이어야
		// 한다. 두 월드의 시간 원점이 다르므로 서버 값을 그대로 쓰면 단발 애니메이션의
		// 경과 시간이 엉뚱하게 계산된다.
		TryEnterActionState(*State, ActionState, ClientWorldTime);
	}

	// 손상도. 클라이언트에는 전투 프로세서가 없으므로 이 경로가 유일한 출처다.
	// 이게 없으면 클라이언트 화면의 적은 아무리 맞아도 영원히 멀쩡해 보인다.
	if (FTREnemyVisualFragment* Visual = EntityManager.GetFragmentDataPtr<FTREnemyVisualFragment>(Entity))
	{
		const float NewRatio = DamageRatio / 255.f;

		// 서버와 같은 규칙 — 값이 올라간 것만 피격으로 본다. 스냅샷 주기보다 짧은
		// 간격으로 두 대 맞으면 한 번만 번쩍이지만, 그 차이는 눈에 보이지 않는다.
		if (NewRatio > Visual->DamageRatio + KINDA_SMALL_NUMBER)
		{
			Visual->LastHitTime = ClientWorldTime;
		}

		Visual->DamageRatio = NewRatio;
	}
}

ATRMassNetRelay::ATRMassNetRelay()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	// 스냅샷은 우리가 직접 주기를 재서 넣는다. 액터 자체는 최대한 자주 내보낼 수 있게 둔다.
	SetNetUpdateFrequency(30.f);
	SetMinNetUpdateFrequency(10.f);
}

void ATRMassNetRelay::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATRMassNetRelay, Agents);
	DOREPLIFETIME(ATRMassNetRelay, ConfigPalette);
}

ATRMassNetRelay* ATRMassNetRelay::Get(UWorld* World, bool bCreateIfMissing)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<ATRMassNetRelay> It(World); It; ++It)
	{
		return *It;
	}

	// 클라이언트는 만들지 않는다. 복제로 도착할 때까지 기다린다.
	if (!bCreateIfMissing || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return World->SpawnActor<ATRMassNetRelay>(ATRMassNetRelay::StaticClass(), SpawnParams);
}

void ATRMassNetRelay::BeginPlay()
{
	Super::BeginPlay();

	// 서버는 스냅샷을 만들기만 하고 반영은 하지 않는다. 자기 엔티티가 이미 진짜다.
	const bool bIsClient = (GetNetMode() == NM_Client);
	SetActorTickEnabled(bIsClient);

	if (bIsClient)
	{
		VerifyClientRepresentationPipeline();
	}
}

void ATRMassNetRelay::VerifyClientRepresentationPipeline() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const EProcessorExecutionFlags WorldFlags = UE::Mass::Utils::GetProcessorExecutionFlagsForWorld(*World);

	// 이 셋이 순서대로 "거리 수집 → LOD 결정 → 표현 적용"이다. 하나라도 빠지면
	// 엔티티는 정상인데 화면에는 아무것도 안 나온다.
	UClass* RequiredProcessors[] =
	{
		UMassLODCollectorProcessor::StaticClass(),
		UMassVisualizationLODProcessor::StaticClass(),
		UMassVisualizationProcessor::StaticClass(),
	};

	for (UClass* ProcessorClass : RequiredProcessors)
	{
		const UMassProcessor* Processor = GetDefault<UMassProcessor>(ProcessorClass);
		if (Processor == nullptr)
		{
			continue;
		}

		const bool bRegistered = Processor->ShouldAutoAddToGlobalList();
		const bool bRunsHere = Processor->ShouldExecute(WorldFlags);

		if (bRegistered && bRunsHere)
		{
			continue;
		}

		UE_LOG(LogTRMass, Error,
			TEXT("[네트 릴레이] '%s' 가 이 클라이언트에서 돌지 않습니다(자동등록 %s, 넷모드 매칭 %s). ")
			TEXT("적 엔티티는 만들어지지만 화면에는 아무것도 그려지지 않습니다. ")
			TEXT("이 컴퓨터의 TR/Config/DefaultMass.ini 에 [%s] 섹션의 ")
			TEXT("bAutoRegisterWithProcessingPhases=True / ExecutionFlags=7 이 있는지 확인하세요."),
			*ProcessorClass->GetName(),
			bRegistered ? TEXT("O") : TEXT("X"),
			bRunsHere ? TEXT("O") : TEXT("X"),
			*ProcessorClass->GetPathName());
	}
}

void ATRMassNetRelay::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 로컬 표현 엔티티를 정리한다. 남겨두면 레벨 전환 후에도 떠 있는다.
	if (UWorld* World = GetWorld())
	{
		if (FMassEntityManager* EntityManager = UE::Mass::Utils::GetEntityManager(World))
		{
			for (const TPair<int32, FMassEntityHandle>& Pair : LocalEntities)
			{
				if (EntityManager->IsEntityValid(Pair.Value))
				{
					EntityManager->Defer().DestroyEntity(Pair.Value);
				}
			}
		}
	}

	LocalEntities.Reset();
	InterpStates.Reset();

	Super::EndPlay(EndPlayReason);
}

uint8 ATRMassNetRelay::RegisterConfig(UMassEntityConfigAsset* Config)
{
	if (Config == nullptr)
	{
		return 0;
	}

	const int32 Existing = ConfigPalette.IndexOfByKey(Config);
	if (Existing != INDEX_NONE)
	{
		return static_cast<uint8>(Existing);
	}

	// 팔레트가 넘치면 0번으로 떨어뜨린다. 표현이 틀릴 뿐 동작은 이어진다.
	if (ConfigPalette.Num() >= MAX_uint8)
	{
		UE_LOG(LogTRMass, Warning, TEXT("네트 릴레이: Config 팔레트가 가득 찼습니다(%d). '%s'는 0번으로 표현됩니다."),
			ConfigPalette.Num(), *GetNameSafe(Config));
		return 0;
	}

	ConfigPalette.Add(Config);

	return static_cast<uint8>(ConfigPalette.Num() - 1);
}

void ATRMassNetRelay::SubmitSnapshot(TArray<FTRMassAgentSnapshot>&& InAgents)
{
	if (!HasAuthority())
	{
		return;
	}

	Agents = MoveTemp(InAgents);

	// 배열 전체를 통째로 갈아끼우므로 더티 표시가 필요하다.
	ForceNetUpdate();
}

void ATRMassNetRelay::OnRep_Agents()
{
	// 새 스냅샷이 왔다. 보간의 시작점을 지금 위치로 옮기고 목표를 갱신한다.
	// 실제 반영은 Tick 에서 한다 — 여기서 엔티티를 만들면 프레임마다 스파이크가 생긴다.
	for (const FTRMassAgentSnapshot& Agent : Agents)
	{
		FInterpState& State = InterpStates.FindOrAdd(Agent.NetId);

		State.PreviousLocation = (State.Alpha >= 1.f) ? State.TargetLocation : State.PreviousLocation;
		State.PreviousYaw = State.TargetYaw;

		State.TargetLocation = Agent.Location;
		State.TargetYaw = static_cast<float>(Agent.Yaw) * (360.f / 256.f);
		State.Alpha = 0.f;
	}
}

void ATRMassNetRelay::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetNetMode() != NM_Client)
	{
		return;
	}

	// 진단을 먼저 남긴다. 아래에서 엔티티 매니저를 못 찾아 빠져나가는 경우까지
	// 로그에 남아야 "릴레이는 왔는데 그 다음이 없다"를 구분할 수 있다.
	LogClientDiagnostics();

	ApplySnapshotToEntities(DeltaSeconds);
}

void ATRMassNetRelay::ApplySnapshotToEntities(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	FMassEntityManager* EntityManager = World ? UE::Mass::Utils::GetEntityManager(World) : nullptr;
	UMassSpawnerSubsystem* Spawner = World ? World->GetSubsystem<UMassSpawnerSubsystem>() : nullptr;

	if (EntityManager == nullptr || Spawner == nullptr)
	{
		return;
	}

	// 스냅샷 사이를 메우는 보간 진행도. 주기보다 조금 빠르게 감아서 다음 스냅샷이
	// 늦어도 목표에 도달해 있게 한다(늦으면 멈춘 것처럼 보이는 편이 낫다).
	const float InterpSpeed = (SnapshotInterval > KINDA_SMALL_NUMBER) ? (1.f / SnapshotInterval) : 10.f;

	TSet<int32> LiveNetIds;
	LiveNetIds.Reserve(Agents.Num());

	for (const FTRMassAgentSnapshot& Agent : Agents)
	{
		if (Agent.NetId == 0)
		{
			continue;
		}

		LiveNetIds.Add(Agent.NetId);

		// 처음 보는 적이면 로컬 표현을 만든다.
		FMassEntityHandle* Existing = LocalEntities.Find(Agent.NetId);
		if (Existing == nullptr || !EntityManager->IsEntityValid(*Existing))
		{
			UMassEntityConfigAsset* Config = ConfigPalette.IsValidIndex(Agent.ConfigIndex)
				? ConfigPalette[Agent.ConfigIndex].Get() : nullptr;
			if (Config == nullptr)
			{
				// 팔레트가 아직 안 왔거나(복제 순서), 애셋 참조가 이 컴퓨터에서 풀리지
				// 않은 경우다. 후자는 다른 컴퓨터끼리 붙을 때만 나오고 조용히 실패한다.
				if (!WarnedConfigIndices.Contains(Agent.ConfigIndex))
				{
					WarnedConfigIndices.Add(Agent.ConfigIndex);

					UE_LOG(LogTRMass, Warning,
						TEXT("[네트 릴레이] 스냅샷의 Config 색인 %d 를 풀 수 없습니다(팔레트 %d개). ")
						TEXT("이 색인의 적은 클라이언트 화면에 나오지 않습니다. ")
						TEXT("팔레트가 계속 0개면 ConfigPalette 복제가 도착하지 않은 것이고, ")
						TEXT("개수는 맞는데 항목이 비어 있으면 그 애셋을 이 컴퓨터가 못 찾은 것입니다."),
						static_cast<int32>(Agent.ConfigIndex), ConfigPalette.Num());
				}

				continue;
			}

			const FMassEntityTemplate& Template = Config->GetOrCreateEntityTemplate(*World);
			if (!Template.IsValid())
			{
				UE_LOG(LogTRMass, Warning, TEXT("[네트 릴레이] '%s' 의 엔티티 템플릿이 유효하지 않습니다."),
					*GetNameSafe(Config));
				continue;
			}

			TArray<FMassEntityHandle> Spawned;
			Spawner->SpawnEntities(Template, 1, Spawned);
			if (Spawned.Num() == 0)
			{
				UE_LOG(LogTRMass, Warning, TEXT("[네트 릴레이] '%s' 로컬 표현 엔티티 스폰에 실패했습니다."),
					*GetNameSafe(Config));
				continue;
			}

			LocalEntities.Add(Agent.NetId, Spawned[0]);
			Existing = LocalEntities.Find(Agent.NetId);

			if (FTRMassNetIdFragment* NetId = EntityManager->GetFragmentDataPtr<FTRMassNetIdFragment>(Spawned[0]))
			{
				NetId->NetId = Agent.NetId;
				NetId->ConfigIndex = Agent.ConfigIndex;
			}

			// 첫 프레임은 보간할 과거가 없다. 스냅샷 위치에 바로 놓는다.
			FInterpState& State = InterpStates.FindOrAdd(Agent.NetId);
			State.PreviousLocation = Agent.Location;
			State.TargetLocation = Agent.Location;
			State.Alpha = 1.f;
		}

		const FMassEntityHandle Entity = *Existing;
		FInterpState& State = InterpStates.FindOrAdd(Agent.NetId);
		State.Alpha = FMath::Min(1.f, State.Alpha + DeltaSeconds * InterpSpeed);

		const FVector NewLocation = FMath::Lerp(State.PreviousLocation, State.TargetLocation, State.Alpha);
		const float NewYaw = FMath::LerpStable(State.PreviousYaw, State.TargetYaw, State.Alpha);

		if (FTransformFragment* TransformFragment = EntityManager->GetFragmentDataPtr<FTransformFragment>(Entity))
		{
			FTransform& Transform = TransformFragment->GetMutableTransform();

			// 속도는 위치 차분으로 만든다. 애니메이션이 대기/이동을 이걸로 가른다.
			if (FMassVelocityFragment* Velocity = EntityManager->GetFragmentDataPtr<FMassVelocityFragment>(Entity))
			{
				Velocity->Value = (DeltaSeconds > KINDA_SMALL_NUMBER)
					? (NewLocation - Transform.GetLocation()) / DeltaSeconds
					: FVector::ZeroVector;
			}

			Transform.SetLocation(NewLocation);
			Transform.SetRotation(FRotator(0.f, NewYaw, 0.f).Quaternion());
		}

		// 보간이 필요 없는 값들은 스냅샷이 스스로 되돌린다. 굽는 쪽(CollectFrom)과
		// 나란히 있어 필드를 늘릴 때 한쪽만 고치는 일을 줄인다.
		Agent.ApplyTo(*EntityManager, Entity, World->GetTimeSeconds());
	}

	DestroyMissingEntities(LiveNetIds);
}

void ATRMassNetRelay::LogClientDiagnostics()
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 5초마다 한 줄. 매 틱 찍으면 로그가 이걸로 도배되고, 더 뜸하면 접속 직후의
	// 몇 초를 놓친다 — 정작 원인은 대부분 그 구간에서 드러난다.
	constexpr float DiagnosticsInterval = 5.f;

	const float WorldTime = World->GetTimeSeconds();
	if (WorldTime < NextDiagnosticsTime)
	{
		return;
	}
	NextDiagnosticsTime = WorldTime + DiagnosticsInterval;

	UE_LOG(LogTRMass, Log,
		TEXT("[네트 릴레이] 클라이언트 상태 — 스냅샷 %d, Config 팔레트 %d, 로컬 엔티티 %d"),
		Agents.Num(), ConfigPalette.Num(), LocalEntities.Num());
}

void ATRMassNetRelay::DestroyMissingEntities(const TSet<int32>& LiveNetIds)
{
	UWorld* World = GetWorld();
	FMassEntityManager* EntityManager = World ? UE::Mass::Utils::GetEntityManager(World) : nullptr;
	if (EntityManager == nullptr)
	{
		return;
	}

	for (auto It = LocalEntities.CreateIterator(); It; ++It)
	{
		if (LiveNetIds.Contains(It.Key()))
		{
			continue;
		}

		if (EntityManager->IsEntityValid(It.Value()))
		{
			EntityManager->Defer().DestroyEntity(It.Value());
		}

		InterpStates.Remove(It.Key());
		It.RemoveCurrent();
	}
}

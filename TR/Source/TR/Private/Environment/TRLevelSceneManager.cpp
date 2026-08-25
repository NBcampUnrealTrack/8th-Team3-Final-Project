// TRLevelSceneManager.cpp

#include "Environment/TRLevelSceneManager.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TRGameState.h"
#include "Debug/TRGameplayLog.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Environment/TRBiomeEnvironment.h"
#include "Environment/TREnvironmentBlenderComponent.h"
#include "Environment/TRGroundMotionSubsystem.h"
#include "Environment/TRLevelSceneChunk.h"
#include "Environment/TRLevelSceneFloor.h"
#include "Environment/TRLevelSceneRailSegment.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/ConstructorHelpers.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

// MPC_TREnvironment 의 스칼라 이름. **에셋의 파라미터 이름과 글자 그대로 같아야 한다.**
// 틀리면 UKismetMaterialLibrary 가 조용히 아무것도 안 하고, 소품만 맑은 날로 남는다.
namespace TREnvironmentParameters
{
	static const FName SnowAmountA(TEXT("SnowAmount"));
	static const FName SnowAmountB(TEXT("SnowAmountB"));
	static const FName WetnessA(TEXT("Wetness"));
	static const FName WetnessB(TEXT("WetnessB"));
	static const FName BiomeBoundary(TEXT("BiomeBoundary"));
	static const FName BiomeAxisX(TEXT("BiomeAxisX"));
	static const FName BiomeAxisY(TEXT("BiomeAxisY"));
	static const FName BiomeBlendWidth(TEXT("BiomeBlendWidth"));
}

// 배경 상태를 화면에 계속 띄운다. 전환이 "언제 시작해서 언제 끝나는지"는 한 장면만
// 봐서는 알 수 없고, 값이 흐르는 것을 봐야 판단이 된다.
static TAutoConsoleVariable<int32> CVarLevelSceneDebug(
	TEXT("TR.Scene.Debug"),
	0,
	TEXT("배경 상태(웨이브/바이옴/지면 전환/날씨)를 화면에 표시한다. 0=끔, 1=켬"),
	ECVF_Cheat);

// 화면 표시가 없는 상황(패키징, 서버 콘솔)에서도 같은 내용을 로그로 받기 위한 명령.
static FAutoConsoleCommandWithWorld GDumpSceneCommand(
	TEXT("TR.Scene.Dump"),
	TEXT("지금 배경 상태(웨이브/바이옴/지면 전환/날씨)를 로그에 한 번 찍는다."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}

		bool bFound = false;
		for (TActorIterator<ATRLevelSceneManager> It(World); It; ++It)
		{
			It->DumpSceneDebug();
			bFound = true;
		}

		if (!bFound)
		{
			UE_LOG(LogTRGameplay, Warning, TEXT("[Scene] 이 월드에 ATRLevelSceneManager 가 없다."));
		}
	}));

// 줄마다 키를 고정하면 매 틱 새 줄이 쌓이지 않고 제자리에서 갱신된다.
static constexpr uint64 SceneDebugMessageKeyBase = 887100;

//생성자
ATRLevelSceneManager::ATRLevelSceneManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetReplicateMovement(false);

	// 프리셋을 하나도 안 채운 레벨에서는 아무 일도 안 한다. 그래서 늘 붙여 둬도
	// 손해가 없고, 쓰려는 사람이 컴포넌트를 추가하는 절차를 밟지 않아도 된다.
	EnvironmentBlender = CreateDefaultSubobject<UTREnvironmentBlenderComponent>(
		TEXT("EnvironmentBlender"));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	ChunkClass = ATRLevelSceneChunk::StaticClass();
	RailSegmentClass = ATRLevelSceneRailSegment::StaticClass();
	FloorClass = ATRLevelSceneFloor::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FloorMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	FloorMesh = FloorMeshFinder.Object;
}

//게임 시작시 환경 메시 청크와 레일, 지면 생성/설정.
void ATRLevelSceneManager::BeginPlay()
{
	Super::BeginPlay();

	// 검증도 청크 생성도 "합쳐진 세트"를 본다. 이걸 먼저 만들어 두지 않으면
	// 둘 다 빈 세트를 보고 엉뚱한 경고를 내거나 소품을 하나도 안 깐다.
	RebuildResolvedMeshDefinitions();
	ValidateGenerationSettings();

	// 첫 청크를 만들기 **전에** 바이옴을 정해야 한다. 나중에 정하면 처음 깔린 청크들이
	// 레거시 세트(바이옴을 쓰는 레벨에서는 대개 비어 있다)로 만들어져, 판이 시작될 때
	// 허허벌판이었다가 재활용이 한 바퀴 돈 뒤에야 풍경이 채워진다.
	//
	// 목록의 첫 항목을 기본값으로 쓴다. 웨이브 테이블 1번 행에 BiomeId 가 적혀 있으면
	// 판이 시작될 때 그 값으로 바뀐다.
	CurrentBiome = DefaultBiome;

	CreateInitialChunks();
	CreateInitialRailSegments();
	CreateFloor();

	// 판이 시작될 때의 바이옴을 지면과 MPC 에 박아 둔다. 첫 전환 전까지는 섞을 것이
	// 없으므로 A 슬롯만 채우고 경계는 화면 밖이다.
	//
	// **지면 액터가 없어도 부른다.** 지면을 안 쓰는 레벨에서도 소품은 깔리고, MPC 가
	// 기본값(맑음)에 머물러 있으면 눈 오는 웨이브에서 바위만 뽀송하다.
	CommitFloorBiome(CurrentBiome, /*bSettleSurface=*/true);
}

//세팅 검증용 코드
void ATRLevelSceneManager::ValidateGenerationSettings() const
{
	//처음 뒤에 배치할 청크 액터 갯수가 전체를 넘어서는지
	const int32 SafeChunkCount = FMath::Max(1, ChunkCount);
	if (InitialBehindChunkCount >= SafeChunkCount)
	{
		UE_LOG(LogTRGameplay, Warning,
			TEXT("[LevelScene] %s: InitialBehindChunkCount(%d) must be less than ChunkCount(%d). Runtime value will be clamped to %d."),
			*GetName(), InitialBehindChunkCount, ChunkCount, SafeChunkCount - 1);
	}

	//처음 뒤에 배치할 레일 액터 갯수가 전체를 넘어서는지
	const int32 SafeRailSegmentCount = FMath::Max(1, RailSegmentCount);
	if (InitialBehindRailSegmentCount >= SafeRailSegmentCount)
	{
		UE_LOG(LogTRGameplay, Warning,
			TEXT("[LevelScene] %s: InitialBehindRailSegmentCount(%d) must be less than RailSegmentCount(%d). Runtime value will be clamped to %d."),
			*GetName(), InitialBehindRailSegmentCount, RailSegmentCount, SafeRailSegmentCount - 1);
	}

	// 소품 세트 검사.
	//
	// **합쳐진 세트를 본다.** 바이옴 전용이 비어 있어도 공통이 깔리면 정상이므로,
	// 어느 한쪽만 보면 멀쩡한 설정에 경고를 내거나 반대로 빈 화면을 놓친다.
	if (Biomes.IsEmpty())
	{
		ValidateMeshDefinitions(SharedMeshDefinitions, TEXT("SharedMeshDefinitions"));
		return;
	}

	for (const TPair<ETRBiomeType, FTRBiomeDefinition>& Pair : Biomes)
	{
		// 일부러 비운 바이옴은 경고하지 않는다. bIgnoreSharedMeshes 를 켜고 전용도
		// 비운 것은 "허허벌판"이라는 명시적 선택이다.
		if (Pair.Value.bIgnoreSharedMeshes && Pair.Value.MeshDefinitions.IsEmpty())
		{
			UE_LOG(LogTRGameplay, Log,
				TEXT("[LevelScene] %s: Biomes[%s] 는 소품 없는 구간이다(공통 제외 + 전용 없음)."),
				*GetName(), *UEnum::GetValueAsString(Pair.Key));
			continue;
		}

		ValidateMeshDefinitions(
			GetBiomeMeshDefinitions(Pair.Key),
			FString::Printf(TEXT("Biomes[%s](공통 포함)"), *UEnum::GetValueAsString(Pair.Key)));
	}
}

//소품 세트 하나를 검사한다
void ATRLevelSceneManager::ValidateMeshDefinitions(
	const TArray<FTRLevelSceneMeshDefinition>& InMeshDefinitions, const FString& Context) const
{
	if (InMeshDefinitions.IsEmpty())
	{
		UE_LOG(LogTRGameplay, Warning,
			TEXT("[LevelScene] %s: %s 가 비어 있다. 소품이 생성되지 않는다. SharedMeshDefinitions 나 이 바이옴의 MeshDefinitions 를 채울 것."),
			*GetName(), *Context);
		return;
	}

	const bool bExcludeRailArea = bGenerateRail && RailSegmentClass && RailMesh;
	const float HalfWidth = FMath::Max(1.f, ChunkSize.Y) * 0.5f;
	const float RailClearance = FMath::Max(0.f, RailPlacementClearanceHalfWidth);
	const float ExclusionMin = RailOffset.Y - RailClearance;
	const float ExclusionMax = RailOffset.Y + RailClearance;

	for (int32 DefinitionIndex = 0; DefinitionIndex < InMeshDefinitions.Num(); ++DefinitionIndex)
	{
		const FTRLevelSceneMeshDefinition& Definition = InMeshDefinitions[DefinitionIndex];

		if (Definition.Mesh == nullptr)
		{
			UE_LOG(LogTRGameplay, Warning,
				TEXT("[LevelScene] %s: %s[%d] 의 Mesh 가 비어 있다. 이 항목은 건너뛴다."),
				*GetName(), *Context, DefinitionIndex);
			continue;
		}

		// **가장 흔한 사고다.** MinInstanceCount/MaxInstanceCount 의 기본값이 0 이라,
		// 메시만 꽂고 개수를 안 채우면 정의는 멀쩡한데 0 개가 생성된다. 설정 화면만
		// 봐서는 다 채운 것처럼 보인다.
		if (FMath::Max(0, Definition.MaxInstanceCount) <= 0)
		{
			UE_LOG(LogTRGameplay, Warning,
				TEXT("[LevelScene] %s: %s[%d](%s) 의 MaxInstanceCount 가 0 이다. 이 소품은 하나도 안 생긴다."),
				*GetName(), *Context, DefinitionIndex, *Definition.Mesh->GetName());
			continue;
		}

		if (!bExcludeRailArea || Definition.PlacementMode != ETRLevelScenePlacementMode::Random)
		{
			continue;
		}

		//LateralRange 스폰 범위가 레일 스폰 금지 구역에 완전 가려지는지 확인
		const float MinY = FMath::Clamp(
			FMath::Min(Definition.LateralRange.X, Definition.LateralRange.Y), -HalfWidth, HalfWidth);
		const float MaxY = FMath::Clamp(
			FMath::Max(Definition.LateralRange.X, Definition.LateralRange.Y), MinY, HalfWidth);

		bool bHasAllowedPlacement = true;
		if (FMath::IsNearlyEqual(MinY, MaxY))
		{
			bHasAllowedPlacement = MinY < ExclusionMin || MinY > ExclusionMax;
		}
		else
		{
			const float OverlapMin = FMath::Max(MinY, ExclusionMin);
			const float OverlapMax = FMath::Min(MaxY, ExclusionMax);
			if (OverlapMin < OverlapMax)
			{
				const float TotalAllowedWidth = FMath::Max(0.f, OverlapMin - MinY)
					+ FMath::Max(0.f, MaxY - OverlapMax);
				bHasAllowedPlacement = TotalAllowedWidth > KINDA_SMALL_NUMBER;
			}
		}

		if (!bHasAllowedPlacement)
		{
			UE_LOG(LogTRGameplay, Warning,
				TEXT("[LevelScene] %s: %s[%d](%s) 가 생성될 수 없다. 배치 범위 [%.0f, %.0f] 가 레일 제외 구간 [%.0f, %.0f] 에 완전히 덮여 있다."),
				*GetName(), *Context, DefinitionIndex, *Definition.Mesh->GetName(),
				MinY, MaxY, ExclusionMin, ExclusionMax);
		}
	}
}

//게임 종료시 관리 대상 액터 전체 삭제
void ATRLevelSceneManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ATRLevelSceneChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			Chunk->Destroy();
		}
	}
	ActiveChunks.Reset();
	for (ATRLevelSceneRailSegment* RailSegment : ActiveRailSegments)
	{
		if (IsValid(RailSegment))
		{
			RailSegment->Destroy();
		}
	}
	ActiveRailSegments.Reset();

	if (IsValid(ActiveFloor))
	{
		ActiveFloor->Destroy();
	}
	ActiveFloor = nullptr;

	Super::EndPlay(EndPlayReason);
}

//틱마다 청크/레일/지면 업데이트 및 필요시 재활용
void ATRLevelSceneManager::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBiomeForWave();
	UpdateFloorBiome(DeltaSeconds);

	// **지면을 갱신한 뒤에 부른다.** 환경 진행도는 이번 프레임의 경계 위치에서 나오므로,
	// 순서가 뒤집히면 하늘이 땅보다 한 프레임 늦게 따라온다.
	UpdateEnvironment(DeltaSeconds);
	UpdateFloorVelocity();
	PublishGroundVelocity();
	DrawSceneDebug();

	if (!IsSceneMovementAllowed() || VisualSpeed <= 0.f)
	{
		return;
	}

	MoveChunks(DeltaSeconds);
	MoveRailSegments(DeltaSeconds);
	RecyclePassedChunks();
	RecyclePassedRailSegments();
}

//GameState 가 알려주는 BiomeId 를 따라간다
void ATRLevelSceneManager::UpdateBiomeForWave()
{
	if (Biomes.IsEmpty())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const ATRGameState* State = World ? World->GetGameState<ATRGameState>() : nullptr;
	if (State == nullptr)
	{
		// 클라이언트에서는 접속 직후 몇 프레임 동안 정상적으로 비어 있다.
		return;
	}

	const ETRBiomeType Desired = State->GetBiome();

	// 근거가 그대로면 결론도 그대로다. 정의 없는 종류 경고가 매 틱 나가는 것도 이걸로 막는다.
	if (Desired == LastEvaluatedBiome)
	{
		return;
	}
	LastEvaluatedBiome = Desired;

	if (!Biomes.Contains(Desired))
	{
		// 테이블에는 지정돼 있는데 이 레벨에 정의가 없다. 조용히 넘어가면
		// "웨이브는 넘어갔는데 풍경만 그대로"로 보여 원인을 찾기 어렵다.
		UE_LOG(LogTRGameplay, Warning,
			TEXT("[LevelScene] 웨이브 %d 가 요구한 풍경 %s 의 정의가 Biomes 에 없다. 풍경을 유지한다."),
			State->GetWaveNumber(), *UEnum::GetValueAsString(Desired));
		return;
	}

	// 웨이브 번호를 같이 남긴다. 풍경만 찍어 두면 나중에 로그를 볼 때 "몇 번째
	// 웨이브에서 바뀐 것인지"를 다른 로그와 시간으로 맞춰 봐야 한다.
	UE_LOG(LogTRGameplay, Log, TEXT("[LevelScene] 웨이브 %d 의 풍경: %s"),
		State->GetWaveNumber(), *UEnum::GetValueAsString(Desired));

	SetCurrentBiome(Desired);
}

//배경 상태를 읽을 수 있는 줄로 만든다
void ATRLevelSceneManager::BuildDebugLines(TArray<FString>& OutLines) const
{
	const UWorld* World = GetWorld();
	const ATRGameState* State = World ? World->GetGameState<ATRGameState>() : nullptr;

	if (State)
	{
		FString RestSuffix;
		if (State->IsResting())
		{
			RestSuffix = FString::Printf(TEXT("  휴식 %.1fs 남음"), State->GetRestTimeRemaining());
		}

		OutLines.Add(FString::Printf(TEXT("[Scene] 웨이브 %d  단계 %s%s"),
			State->GetWaveNumber(),
			*UEnum::GetValueAsString(State->GetWavePhase()),
			*RestSuffix));

		// 테이블이 요구한 값과 이 액터가 실제로 쓰는 값을 **따로 찍는다.** 둘이 다르면
		// 정의가 빠졌거나 복제가 안 온 것이고, 한 줄로 합쳐 놓으면 그 구별이 사라진다.
		OutLines.Add(FString::Printf(TEXT("[Scene] 테이블 %s  ->  소품 %s  지면 %s"),
			*UEnum::GetValueAsString(State->GetBiome()),
			*UEnum::GetValueAsString(CurrentBiome),
			*UEnum::GetValueAsString(FloorBiome)));
	}
	else
	{
		OutLines.Add(TEXT("[Scene] GameState 없음 — 접속 직후이거나 이 월드에 없다"));
		OutLines.Add(FString::Printf(TEXT("[Scene] 소품 %s  지면 %s"),
			*UEnum::GetValueAsString(CurrentBiome),
			*UEnum::GetValueAsString(FloorBiome)));
	}

	if (bFloorBlending)
	{
		OutLines.Add(FString::Printf(TEXT("[Scene] 지면 전환중 -> %s  경계 %.0fcm (%.0fcm 에서 끝)"),
			*UEnum::GetValueAsString(FloorBlendTarget),
			FloorBoundaryTravel,
			-FMath::Max(0.f, FloorBoundaryOvershoot)));
	}
	else
	{
		OutLines.Add(TEXT("[Scene] 지면 전환중 아님"));
	}

	const FVector2D Effect = GetCurrentFloorSurfaceEffect();
	OutLines.Add(FString::Printf(
		TEXT("[Scene] 눈 %.2f->%.2f  젖음 %.2f->%.2f  차오름 %.0f%%  슬롯 %s"),
		Effect.X, FloorSurfaceTarget.X,
		Effect.Y, FloorSurfaceTarget.Y,
		FloorSurfaceProgress * 100.f,
		bFloorBlending ? TEXT("B") : TEXT("A")));

	// **목표 프리셋이 비어 있는지를 같이 찍는 것이 요점이다.** "안개가 안 변한다"의
	// 원인은 대개 프리셋을 안 물린 것인데, 진행도만 찍으면 100% 라고 나와서 정상으로
	// 보인다. 무엇에서 무엇으로 가는지와 그 목표가 실제로 있는지를 함께 봐야 한다.
	OutLines.Add(FString::Printf(TEXT("[Scene] 환경 %s -> %s  %.0f%% (%s)%s"),
		*UEnum::GetValueAsString(EnvironmentFromBiome),
		*UEnum::GetValueAsString(EnvironmentToBiome),
		EnvironmentAlpha * 100.f,
		bFloorBlending ? TEXT("경계 기준") : TEXT("시간 기준"),
		GetBiomeEnvironment(EnvironmentToBiome)
			? TEXT("")
			: TEXT("  ** 목표 프리셋 없음 - 환경 안 바뀜 **")));

	// **정의 수와 인스턴스 수를 같이 찍는 것이 요점이다.** 정의는 채워져 있는데
	// 인스턴스가 0 이면 개수 설정이나 배치 제외 구간 문제이고, 정의가 0 이면
	// 데이터 자체가 안 들어온 것이다. 둘은 고치는 곳이 완전히 다르다.
	int32 TotalInstances = 0;
	int32 ChunksWithCurrentBiome = 0;
	for (const ATRLevelSceneChunk* Chunk : ActiveChunks)
	{
		if (!IsValid(Chunk))
		{
			continue;
		}

		TotalInstances += Chunk->GetTotalInstanceCount();
		if (Chunk->GetBiome() == CurrentBiome)
		{
			++ChunksWithCurrentBiome;
		}
	}

	OutLines.Add(FString::Printf(TEXT("[Scene] 소품: 정의 %d개  인스턴스 %d개  청크 %d/%d개가 현재 풍경"),
		GetBiomeMeshDefinitions(CurrentBiome).Num(),
		TotalInstances,
		ChunksWithCurrentBiome,
		ActiveChunks.Num()));

	OutLines.Add(FString::Printf(TEXT("[Scene] 속도 %.0f  이동 %s  바이옴 정의 %d개"),
		VisualSpeed,
		IsSceneMovementAllowed() ? TEXT("O") : TEXT("X"),
		Biomes.Num()));
}

//화면 표시
void ATRLevelSceneManager::DrawSceneDebug() const
{
	if (CVarLevelSceneDebug.GetValueOnGameThread() == 0 || GEngine == nullptr)
	{
		return;
	}

	TArray<FString> Lines;
	BuildDebugLines(Lines);

	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		// 표시 시간을 0 으로 두면 한 프레임만 남는다. 매 틱 다시 넣으므로 계속 보이고,
		// 끄는 순간 바로 사라진다 — 긴 시간을 주면 끈 뒤에도 한참 화면에 남는다.
		GEngine->AddOnScreenDebugMessage(
			SceneDebugMessageKeyBase + Index, 0.f, FColor::Cyan, Lines[Index]);
	}
}

//로그로 한 번 찍는다
void ATRLevelSceneManager::DumpSceneDebug() const
{
	TArray<FString> Lines;
	BuildDebugLines(Lines);

	for (const FString& Line : Lines)
	{
		UE_LOG(LogTRGameplay, Log, TEXT("%s"), *Line);
	}
}

//지면 텍스처를 소품 전환에 맞춰 따라가게 한다
void ATRLevelSceneManager::UpdateFloorBiome(const float DeltaSeconds)
{
	if (!IsValid(ActiveFloor) || Biomes.IsEmpty())
	{
		return;
	}

	// 날씨는 경계와 무관하게 제 속도로 차오른다. 전환 중이든 아니든 매 틱 돈다.
	// 지금 차오르는 슬롯은 전환 여부가 정한다 — 전환 중이면 경계 너머의 B 다.
	UpdateFloorSurfaceEffect(DeltaSeconds, /*bSlotB=*/bFloorBlending);

	// ── 전환 중이면 경계를 기차 쪽으로 옮긴다 ──────────────────────────────
	if (bFloorBlending)
	{
		// **소품과 같은 식으로 움직인다.** 청크도 매 틱 같은 속도 × DeltaTime 만큼
		// 밀리므로, 경계선과 소품이 정확히 같은 속도로 다가온다. 시간 기반으로 하면
		// 속도가 바뀌는 순간 둘이 어긋나 지형과 소품의 경계가 따로 논다.
		FloorBoundaryTravel -= GetEffectiveVisualSpeed() * DeltaSeconds;
		PushFloorBoundary(FloorBoundaryTravel);

		// 경계가 기차를 충분히 지나쳤으면 끝난 것이다. 이제 화면에 보이는 지면은
		// 전부 새 풍경이므로, A 슬롯을 새 텍스처로 바꿔도 티가 나지 않는다.
		if (FloorBoundaryTravel <= -FMath::Max(0.f, FloorBoundaryOvershoot))
		{
			bFloorBlending = false;
			CommitFloorBiome(FloorBlendTarget);
		}

		// 전환 중에는 새 목표를 받지 않는다. 다음 틱에 위 조건이 풀리면 자연스럽게
		// 다음 전환이 시작되므로, 따로 대기열을 둘 필요가 없다.
		return;
	}

	// ── 지면이 소품을 따라잡았으면 할 일이 없다 ────────────────────────────
	if (FloorBiome == CurrentBiome)
	{
		return;
	}

	// 지면 텍스처가 없는 풍경은 "지면 유지"로 본다. 섞을 것이 없으므로 값만 맞춘다.
	const FTRBiomeDefinition* Target = Biomes.Find(CurrentBiome);
	if (Target == nullptr || !Target->HasFloorTextures())
	{
		FloorBiome = CurrentBiome;

		// 다만 날씨는 여전히 바뀔 수 있다. 같은 땅에 비가 오기 시작하는 구간이
		// 그렇다 — 텍스처를 갈아 끼울 이유가 없으니 경계선도 필요 없고, 지금 보이는
		// 지면 위에서 그대로 젖어 들면 된다. 그래서 A 슬롯을 제자리에서 올린다.
		StartFloorSurfaceEffect(CurrentBiome, GetCurrentFloorSurfaceEffect(), /*bImmediate=*/false,
			/*bSlotB=*/false);
		return;
	}

	// ── 새 소품이 도착할 때까지 기다린다 ───────────────────────────────────
	if (!HasBiomeReachedTrain(CurrentBiome))
	{
		return;
	}

	ActiveFloor->SetBiomeTextures(Target->FloorBaseColor, Target->FloorNormal, /*bSlotB=*/true);

	FloorBlendTarget = CurrentBiome;
	bFloorBlending = true;

	// B 슬롯의 날씨는 맑음에서 출발한다. 경계 너머로 갓 드러난 땅에 이미 눈이
	// 두껍게 쌓여 있으면 "도착했다"가 아니라 "다른 그림으로 갈아 끼웠다"가 된다.
	StartFloorSurfaceEffect(CurrentBiome, FVector2D::ZeroVector, /*bImmediate=*/false, /*bSlotB=*/true);

	// 경계는 가장 앞선 청크 자리에서 출발한다. 그보다 앞은 어차피 안 보이고,
	// 그보다 뒤에서 시작하면 이미 눈에 들어와 있는 지면이 갑자기 갈린다.
	FloorBoundaryTravel = FMath::Max(GetFrontmostTravelDistance(), 0.f);
	PushFloorBoundary(FloorBoundaryTravel);

	UE_LOG(LogTRGameplay, Log, TEXT("[LevelScene] 지면 전환 시작: %s (경계 %.0fcm 앞에서 출발)"),
		*UEnum::GetValueAsString(CurrentBiome), FloorBoundaryTravel);
}

//환경(안개/해/하늘빛/PP)을 지면 전환에 맞춰 따라가게 한다
void ATRLevelSceneManager::UpdateEnvironment(const float DeltaSeconds)
{
	if (EnvironmentBlender == nullptr)
	{
		return;
	}

	// 목표는 지면 슬롯을 그대로 따라간다. 전환 중이면 경계 너머(B), 아니면 지금 풍경(A).
	const ETRBiomeType DesiredTo = bFloorBlending ? FloorBlendTarget : FloorBiome;

	if (DesiredTo != EnvironmentToBiome)
	{
		// 새 목표가 생겼다. 지금 보이던 것에서 출발한다.
		EnvironmentFromBiome = EnvironmentToBiome;
		EnvironmentToBiome = DesiredTo;
		EnvironmentAlpha = 0.f;
	}

	const UTRBiomeEnvironment* Target = GetBiomeEnvironment(EnvironmentToBiome);

	if (bFloorBlending)
	{
		EnvironmentAlpha = GetEnvironmentBlendAlpha();
	}
	else
	{
		// 따라갈 선이 없는 구간이다. 여기서만 시간으로 간다.
		const float BlendSeconds = Target ? FMath::Max(0.f, Target->BlendSeconds) : 0.f;
		EnvironmentAlpha = BlendSeconds > 0.f
			? FMath::Min(EnvironmentAlpha + DeltaSeconds / BlendSeconds, 1.f)
			: 1.f;
	}

	EnvironmentBlender->ApplyBlend(
		GetBiomeEnvironment(EnvironmentFromBiome), Target, EnvironmentAlpha);
}

//경계 전환의 진행도. 기차 발밑 지면이 얼마나 새 풍경인가
float ATRLevelSceneManager::GetEnvironmentBlendAlpha() const
{
	return FMath::Clamp(-FloorBoundaryTravel / GetSafeFloorBlendWidth(), 0.f, 1.f);
}

//그 풍경의 환경 프리셋
const UTRBiomeEnvironment* ATRLevelSceneManager::GetBiomeEnvironment(const ETRBiomeType InBiome) const
{
	const FTRBiomeDefinition* Found = Biomes.Find(InBiome);
	return Found ? Found->Environment.Get() : nullptr;
}

//경계 위치를 지면과 소품 양쪽에 밀어 넣는다
void ATRLevelSceneManager::PushFloorBoundary(const float TravelBoundary) const
{
	// 머테리얼은 픽셀의 절대 월드 좌표를 축에 투영한다. 그래서 매니저 기준 거리에
	// 매니저 자신의 투영값을 더해 같은 축으로 옮겨 준다.
	const FVector Forward = GetActorForwardVector();
	const FVector2D Axis(Forward.X, Forward.Y);
	const FVector2D SafeAxis = Axis.IsNearlyZero() ? FVector2D(1.f, 0.f) : Axis.GetSafeNormal();

	const FVector Origin = GetActorLocation();
	const float OriginProjection = static_cast<float>(Origin.X * SafeAxis.X + Origin.Y * SafeAxis.Y);
	const float WorldBoundary = OriginProjection + TravelBoundary;

	if (IsValid(ActiveFloor))
	{
		ActiveFloor->SetBiomeBoundary(WorldBoundary, SafeAxis, GetSafeFloorBlendWidth());
	}

	// 소품은 지면 액터와 무관하게 존재한다. 지면을 안 쓰는 레벨에서도 소품에는 눈이
	// 와야 하므로, 위 분기와 상관없이 늘 보낸다.
	PushEnvironmentScalar(TREnvironmentParameters::BiomeBoundary, WorldBoundary);
	PushEnvironmentScalar(TREnvironmentParameters::BiomeAxisX, static_cast<float>(SafeAxis.X));
	PushEnvironmentScalar(TREnvironmentParameters::BiomeAxisY, static_cast<float>(SafeAxis.Y));

	// 0 으로 나누면 마스크가 NaN 이 되어 소품이 통째로 사라진다. 지면 쪽 클램프는
	// ATRLevelSceneFloor 안에 있으므로, 여기서 한 번 더 막아 준다.
	PushEnvironmentScalar(TREnvironmentParameters::BiomeBlendWidth, GetSafeFloorBlendWidth());
}

//MPC 스칼라 하나를 밀어 넣는다
void ATRLevelSceneManager::PushEnvironmentScalar(const FName ParameterName, const float Value) const
{
	if (EnvironmentCollection == nullptr)
	{
		return;
	}

	// this 를 넘기면 const 가 벗겨지지 않는다. 월드를 직접 넘긴다 — MPC 값은 어차피
	// 액터가 아니라 월드에 붙는다.
	UKismetMaterialLibrary::SetScalarParameterValue(
		GetWorld(), EnvironmentCollection.Get(), ParameterName, Value);
}

//새 바이옴 청크가 기차에 닿았는지
bool ATRLevelSceneManager::HasBiomeReachedTrain(const ETRBiomeType InBiome) const
{
	// 기차는 매니저 원점에 고정돼 있다. 주행 거리가 0 이하면 그 청크는 기차를 지나고 있다.
	//
	// 시간으로 계산하지 않는 이유는 속도가 바뀔 수 있어서다. 실제 도착을 보면
	// VisualSpeed 를 올리든 잠시 멈추든 저절로 맞는다.
	for (const ATRLevelSceneChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk) && Chunk->GetBiome() == InBiome && GetTravelDistance(Chunk) <= 0.f)
		{
			return true;
		}
	}

	return false;
}

//지면 A 슬롯을 이 바이옴으로 확정
void ATRLevelSceneManager::CommitFloorBiome(const ETRBiomeType InBiome, const bool bSettleSurface)
{
	FloorBiome = InBiome;

	if (!IsValid(ActiveFloor))
	{
		return;
	}

	if (const FTRBiomeDefinition* Biome = Biomes.Find(InBiome))
	{
		ActiveFloor->SetBiomeTextures(Biome->FloorBaseColor, Biome->FloorNormal, /*bSlotB=*/false);
	}

	// 여기서 확정하는 것은 언제나 A 슬롯이다. 전환이 끝났든 레벨을 막 열었든,
	// 이 함수를 지나면 화면에 보이는 지면은 전부 A 다.
	if (bSettleSurface)
	{
		StartFloorSurfaceEffect(InBiome, FVector2D::ZeroVector, /*bImmediate=*/true, /*bSlotB=*/false);
	}
	else
	{
		// 전환이 끝났다. B 에서 차오르던 값을 A 가 그대로 이어받는다. 진행도를
		// 건드리지 않으므로, 아직 다 안 쌓였으면 A 슬롯에서 계속 쌓인다.
		PushFloorSurfaceEffect(/*bSlotB=*/false);
	}

	// 경계를 화면 밖으로 치운다. 이러면 마스크가 어디서나 0 이라 A 슬롯만 보인다.
	// 값을 0 으로 두면 매니저 앞쪽 절반이 B 로 보이는 사고가 난다.
	FloorBoundaryTravel = NoTransitionBoundaryTravel;
	PushFloorBoundary(FloorBoundaryTravel);
}

//날씨 효과를 목표치까지 서서히 채운다
void ATRLevelSceneManager::UpdateFloorSurfaceEffect(const float DeltaSeconds, const bool bSlotB)
{
	if (FloorSurfaceProgress >= 1.f)
	{
		return;
	}

	// 지형 스크롤과 달리 **시간으로 간다.** 눈이 쌓이는 속도는 기차가 얼마나 빨리
	// 달리는지와 상관이 없다. 경계선처럼 속도에 묶으면 정차 중에는 눈이 영영 안 쌓인다.
	FloorSurfaceProgress = FloorSurfaceDuration > 0.f
		? FMath::Min(FloorSurfaceProgress + DeltaSeconds / FloorSurfaceDuration, 1.f)
		: 1.f;

	PushFloorSurfaceEffect(bSlotB);
}

//새 목표 날씨로 차오르기 시작
void ATRLevelSceneManager::StartFloorSurfaceEffect(const ETRBiomeType InBiome, const FVector2D Start,
	const bool bImmediate, const bool bSlotB)
{
	const FTRBiomeDefinition* Biome = Biomes.Find(InBiome);

	// 정의가 없는 풍경은 맑음으로 본다. 이전 바이옴의 눈이 남아 있는 것보다
	// 아무 효과도 없는 편이 사고로 보이지 않는다.
	FloorSurfaceStart = Start;
	FloorSurfaceTarget = Biome ? Biome->GetSurfaceEffect() : FVector2D::ZeroVector;
	FloorSurfaceDuration = Biome && !bImmediate ? FMath::Max(0.f, Biome->SurfaceAccumulationSeconds) : 0.f;
	FloorSurfaceProgress = FloorSurfaceDuration > 0.f ? 0.f : 1.f;

	PushFloorSurfaceEffect(bSlotB);
}

//지금 진행도의 날씨값을 지정한 슬롯에 밀어 넣는다
void ATRLevelSceneManager::PushFloorSurfaceEffect(const bool bSlotB) const
{
	const FVector2D Effect = GetCurrentFloorSurfaceEffect();
	const float Snow = FMath::Clamp(static_cast<float>(Effect.X), 0.f, 1.f);
	const float Wet = FMath::Clamp(static_cast<float>(Effect.Y), 0.f, 1.f);

	if (IsValid(ActiveFloor))
	{
		ActiveFloor->SetBiomeSurfaceEffect(Snow, Wet, bSlotB);
	}

	// 소품도 같은 슬롯에 같은 값을 받는다. 여기가 갈리면 경계선 한쪽에서 땅과 바위의
	// 눈 양이 달라진다.
	PushEnvironmentScalar(
		bSlotB ? TREnvironmentParameters::SnowAmountB : TREnvironmentParameters::SnowAmountA, Snow);
	PushEnvironmentScalar(
		bSlotB ? TREnvironmentParameters::WetnessB : TREnvironmentParameters::WetnessA, Wet);
}

//바이옴 지정. 앞으로 만들어지는 청크부터 적용된다
void ATRLevelSceneManager::SetCurrentBiome(const ETRBiomeType InBiome)
{
	if (InBiome == CurrentBiome)
	{
		return;
	}

	CurrentBiome = InBiome;

	UE_LOG(LogTRGameplay, Log, TEXT("[LevelScene] 바이옴 전환: %s (앞쪽 청크부터 적용)"),
		*UEnum::GetValueAsString(InBiome));
}

//그 풍경에 실제로 깔릴 소품 세트
const TArray<FTRLevelSceneMeshDefinition>& ATRLevelSceneManager::GetBiomeMeshDefinitions(const ETRBiomeType InBiome) const
{
	// 정의가 없는 풍경은 공통 소품만 깐다. 아무것도 안 깔면 웨이브 테이블에 오타가
	// 하나 들어간 것만으로 배경이 통째로 사라진다.
	const FTRLevelSceneMeshDefinitionSet* Found = ResolvedMeshDefinitions.Find(InBiome);
	return Found ? Found->Definitions : SharedMeshDefinitions;
}

//바이옴마다 공통 + 전용을 미리 합쳐 둔다
void ATRLevelSceneManager::RebuildResolvedMeshDefinitions()
{
	ResolvedMeshDefinitions.Reset();
	ResolvedMeshDefinitions.Reserve(Biomes.Num());

	for (const TPair<ETRBiomeType, FTRBiomeDefinition>& Pair : Biomes)
	{
		FTRLevelSceneMeshDefinitionSet& Resolved = ResolvedMeshDefinitions.Add(Pair.Key);

		// **공통을 먼저 넣는다.** 순서가 곧 배치 정의 인덱스이고, 인덱스는 시드에
		// 섞여 들어간다. 공통이 앞에 고정돼 있으면 바이옴 전용을 추가해도 공통 소품의
		// 배치가 그대로 유지된다 — 뒤에 넣으면 전용을 하나 추가할 때마다 공통 배치가
		// 통째로 달라진다.
		if (!Pair.Value.bIgnoreSharedMeshes)
		{
			Resolved.Definitions = SharedMeshDefinitions;
		}

		Resolved.Definitions.Append(Pair.Value.MeshDefinitions);
	}
}

//배경을 흘려도 되는 상태인지 판정
bool ATRLevelSceneManager::IsSceneMovementAllowed() const
{
	if (!bAutoMove)
	{
		return false;
	}

	if (!bMoveOnlyAfterMatchStart)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const ATRGameState* State = World ? World->GetGameState<ATRGameState>() : nullptr;

	return State != nullptr && State->IsMatchStarted();
}

//이번 프레임에 지형이 실제로 흐르는 속도
float ATRLevelSceneManager::GetEffectiveVisualSpeed() const
{
	return IsSceneMovementAllowed() ? FMath::Max(0.f, VisualSpeed) : 0.f;
}

//이동속도 설정
void ATRLevelSceneManager::SetVisualSpeed(const float NewVisualSpeed)
{
	VisualSpeed = FMath::Max(0.f, NewVisualSpeed);
	UpdateFloorVelocity(true);
}

//레일 조각 액터 생성
void ATRLevelSceneManager::CreateInitialRailSegments()
{
	if (!bGenerateRail || !GetWorld() || !RailSegmentClass || !RailMesh)
	{
		return;
	}

	const int32 SafeSegmentCount = FMath::Max(1, RailSegmentCount);
	const int32 SafeBehindCount = FMath::Clamp(InitialBehindRailSegmentCount, 0, SafeSegmentCount - 1);
	const float SafeSegmentLength = FMath::Max(1.f, RailSegmentLength);
	const FVector TravelDirection = GetActorForwardVector();
	const FVector WorldRailOffset = GetActorTransform().TransformVectorNoScale(RailOffset);

	ActiveRailSegments.Reserve(SafeSegmentCount);
	for (int32 SegmentIndex = 0; SegmentIndex < SafeSegmentCount; ++SegmentIndex)
	{
		const float TravelOffset = static_cast<float>(SegmentIndex - SafeBehindCount) * SafeSegmentLength;
		const FTransform SpawnTransform(GetActorRotation(),
			GetActorLocation() + WorldRailOffset + TravelDirection * TravelOffset);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATRLevelSceneRailSegment* RailSegment = GetWorld()->SpawnActor<ATRLevelSceneRailSegment>(
			RailSegmentClass, SpawnTransform, SpawnParameters);
		if (RailSegment)
		{
			RailSegment->InitializeRail(RailMesh, RailEndCullDistance, bRailCastShadow);
			ActiveRailSegments.Add(RailSegment);
		}
	}
}

//지면 액터 생성
void ATRLevelSceneManager::CreateFloor()
{
	if (!bGenerateFloor || !GetWorld() || !FloorClass)
	{
		return;
	}

	//설정되어 있으면 이미 존재하는 액터로 설정
	UStaticMeshComponent* ExistingFloorMeshComponent = nullptr;
	if (bUseExistingLevelFloor && IsValid(ExistingFloorActor))
	{
		ExistingFloorMeshComponent = ExistingFloorActor->FindComponentByClass<UStaticMeshComponent>();
	}

	if (!ExistingFloorMeshComponent && !FloorMesh)
	{
		return;
	}

	const FTransform SpawnTransform = ExistingFloorMeshComponent
		? FTransform::Identity
		: FloorRelativeTransform * GetActorTransform();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveFloor = GetWorld()->SpawnActor<ATRLevelSceneFloor>(FloorClass, SpawnTransform, SpawnParameters);
	if (ActiveFloor)
	{
		if (ExistingFloorMeshComponent)
		{
			ActiveFloor->InitializeExistingFloor(ExistingFloorMeshComponent);
			UE_LOG(LogTRGameplay, Log, TEXT("[LevelScene] 지면 머테리얼 대상: 레벨에 이미 있는 액터 %s"),
				*ExistingFloorActor->GetName());
		}
		else
		{
			ActiveFloor->InitializeFloor(FloorMesh, FloorMaterial);
			UE_LOG(LogTRGameplay, Log, TEXT("[LevelScene] 지면 머테리얼 대상: 새로 만든 지면 판"));
		}
		UpdateFloorVelocity(true);
	}
}

//기본 시드값 설정 및 필요시 청크 초기화
void ATRLevelSceneManager::SetRouteSeed(const int32 NewRouteSeed, const bool bRegenerate)
{
	RouteSeed = NewRouteSeed;
	if (bRegenerate)
	{
		RegenerateAllChunks();
	}
}

//레일 조각 액터 속도만큼 이동
void ATRLevelSceneManager::MoveRailSegments(const float DeltaSeconds)
{
	const FVector MovementDelta = -GetActorForwardVector() * VisualSpeed * DeltaSeconds;
	for (ATRLevelSceneRailSegment* RailSegment : ActiveRailSegments)
	{
		if (IsValid(RailSegment))
		{
			RailSegment->AddActorWorldOffset(MovementDelta, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

//활성화된 청크들의 ISM 인스턴스 리셋, 청크 위치는 변경하지 않음.
void ATRLevelSceneManager::RegenerateAllChunks()
{
	for (ATRLevelSceneChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			Chunk->GenerateChunk(MakeChunkSeed(Chunk->GetSegmentIndex()), Chunk->GetSegmentIndex());
		}
	}
}

//지나간 레일 액터 확인
void ATRLevelSceneManager::RecyclePassedRailSegments()
{
	const float SafeSegmentLength = FMath::Max(1.f, RailSegmentLength);
	const float RecycleThreshold = -FMath::Max(0.f, RailRecycleBehindDistance) - SafeSegmentLength * 0.5f;
	int32 RecycledSegmentCount = 0;

	while (RecycledSegmentCount < ActiveRailSegments.Num())
	{
		ATRLevelSceneRailSegment* PassedSegment = nullptr;
		for (ATRLevelSceneRailSegment* RailSegment : ActiveRailSegments)
		{
			if (IsValid(RailSegment) && GetRailTravelDistance(RailSegment) < RecycleThreshold)
			{
				PassedSegment = RailSegment;
				break;
			}
		}

		if (!PassedSegment)
		{
			break;
		}

		RecycleRailSegment(PassedSegment);
		++RecycledSegmentCount;
	}
}

//청크 초기 세팅
void ATRLevelSceneManager::CreateInitialChunks()
{
	if (!GetWorld() || !ChunkClass)
	{
		return;
	}

	const int32 SafeChunkCount = FMath::Max(1, ChunkCount);
	const int32 SafeBehindCount = FMath::Clamp(InitialBehindChunkCount, 0, SafeChunkCount - 1);
	const float ChunkLength = FMath::Max(1.f, ChunkSize.X);
	const FVector TravelDirection = GetActorForwardVector();
	const bool bExcludeRailArea = bGenerateRail && RailSegmentClass && RailMesh;
	const FVector2D RailLateralExclusionRange(
		RailOffset.Y - FMath::Max(0.f, RailPlacementClearanceHalfWidth),
		RailOffset.Y + FMath::Max(0.f, RailPlacementClearanceHalfWidth));

	ActiveChunks.Reserve(SafeChunkCount);
	for (int32 ChunkIndex = 0; ChunkIndex < SafeChunkCount; ++ChunkIndex)
	{
		const float TravelOffset = static_cast<float>(ChunkIndex - SafeBehindCount) * ChunkLength;
		const FTransform SpawnTransform(GetActorRotation(), GetActorLocation() + TravelDirection * TravelOffset);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATRLevelSceneChunk* Chunk = GetWorld()->SpawnActor<ATRLevelSceneChunk>(ChunkClass, SpawnTransform, SpawnParameters);
		if (!Chunk)
		{
			continue;
		}

		const int32 SegmentIndex = ChunkIndex;
		Chunk->ApplyBiome(CurrentBiome, GetBiomeMeshDefinitions(CurrentBiome));
		Chunk->InitializeChunk(GetBiomeMeshDefinitions(CurrentBiome), ChunkSize, bExcludeRailArea,
			RailLateralExclusionRange);
		Chunk->GenerateChunk(MakeChunkSeed(SegmentIndex), SegmentIndex);
		ActiveChunks.Add(Chunk);
	}

	NextSegmentIndex = SafeChunkCount;
}

//레일 액터 재활용하여 앞쪽에 다시 배치
void ATRLevelSceneManager::RecycleRailSegment(ATRLevelSceneRailSegment* RailSegment)
{
	if (!IsValid(RailSegment))
	{
		return;
	}

	const float NewTravelDistance = GetFrontmostRailTravelDistance(RailSegment) + FMath::Max(1.f, RailSegmentLength);
	const FVector WorldRailOffset = GetActorTransform().TransformVectorNoScale(RailOffset);
	const FVector NewLocation = GetActorLocation() + WorldRailOffset + GetActorForwardVector() * NewTravelDistance;
	RailSegment->SetActorLocationAndRotation(NewLocation, GetActorRotation(), false, nullptr,
		ETeleportType::TeleportPhysics);
}

//지면 머티리얼 속도 파라미터 업데이트
void ATRLevelSceneManager::UpdateFloorVelocity(const bool bForceUpdate)
{
	if (!IsValid(ActiveFloor))
	{
		return;
	}

	const FVector2D SafeDirection = FloorMovementDirection.GetSafeNormal();
	const float EffectiveVisualSpeed = GetEffectiveVisualSpeed();
	if (!bForceUpdate
		&& FMath::IsNearlyEqual(LastAppliedFloorVisualSpeed, EffectiveVisualSpeed)
		&& LastAppliedFloorMovementDirection.Equals(SafeDirection))
	{
		return;
	}

	const FVector2D MaterialVelocity = SafeDirection * EffectiveVisualSpeed;
	ActiveFloor->SetMovementVelocity(MaterialVelocity.X, MaterialVelocity.Y);
	LastAppliedFloorVisualSpeed = EffectiveVisualSpeed;
	LastAppliedFloorMovementDirection = SafeDirection;
}

//지면 스크롤 속도를 Mass 쪽에 알린다
void ATRLevelSceneManager::PublishGroundVelocity() const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UTRGroundMotionSubsystem* GroundMotion = World->GetSubsystem<UTRGroundMotionSubsystem>();
	if (GroundMotion == nullptr)
	{
		return;
	}

	//MoveChunks와 같은 식이어야 한다. 어긋나면 적의 발이 실제 지면과 다른 속도로 움직인다
	GroundMotion->SetGroundVelocity(-GetActorForwardVector() * GetEffectiveVisualSpeed());
}

//청크 속도만큼 이동
void ATRLevelSceneManager::MoveChunks(const float DeltaSeconds)
{
	const FVector MovementDelta = -GetActorForwardVector() * VisualSpeed * DeltaSeconds;
	for (ATRLevelSceneChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			Chunk->AddActorWorldOffset(MovementDelta, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

//매니저 액터 기준 레일 액터의 위치, 앞인지 뒤인지 계산
float ATRLevelSceneManager::GetRailTravelDistance(const ATRLevelSceneRailSegment* RailSegment) const
{
	if (!IsValid(RailSegment))
	{
		return -TNumericLimits<float>::Max();
	}

	const FVector WorldRailOffset = GetActorTransform().TransformVectorNoScale(RailOffset);
	return FVector::DotProduct(RailSegment->GetActorLocation() - GetActorLocation() - WorldRailOffset, GetActorForwardVector());
}

//지나간 청크 액터 확인
void ATRLevelSceneManager::RecyclePassedChunks()
{
	const float RecycleThreshold = -FMath::Max(0.f, RecycleBehindDistance) - FMath::Max(1.f, ChunkSize.X) * 0.5f;
	int32 RecycledChunkCount = 0;

	while (RecycledChunkCount < ActiveChunks.Num())
	{
		ATRLevelSceneChunk* PassedChunk = nullptr;
		for (ATRLevelSceneChunk* Chunk : ActiveChunks)
		{
			if (IsValid(Chunk) && GetTravelDistance(Chunk) < RecycleThreshold)
			{
				PassedChunk = Chunk;
				break;
			}
		}

		if (!PassedChunk)
		{
			break;
		}

		RecycleChunk(PassedChunk);
		++RecycledChunkCount;
	}
}

//가장 앞에 있는 레일 액터까지의 거리 확인
float ATRLevelSceneManager::GetFrontmostRailTravelDistance(const ATRLevelSceneRailSegment* IgnoredRailSegment) const
{
	float FrontmostDistance = 0.f;
	bool bFoundSegment = false;
	for (const ATRLevelSceneRailSegment* RailSegment : ActiveRailSegments)
	{
		if (!IsValid(RailSegment) || RailSegment == IgnoredRailSegment)
		{
			continue;
		}

		FrontmostDistance = bFoundSegment
			? FMath::Max(FrontmostDistance, GetRailTravelDistance(RailSegment))
			: GetRailTravelDistance(RailSegment);
		bFoundSegment = true;
	}

	return bFoundSegment ? FrontmostDistance : 0.f;
}

//청크 액터 재활용해 앞에 다시 배치
void ATRLevelSceneManager::RecycleChunk(ATRLevelSceneChunk* Chunk)
{
	if (!IsValid(Chunk))
	{
		return;
	}

	const float ChunkLength = FMath::Max(1.f, ChunkSize.X);
	const float NewTravelDistance = GetFrontmostTravelDistance(Chunk) + ChunkLength;
	const FVector NewLocation = GetActorLocation() + GetActorForwardVector() * NewTravelDistance;
	Chunk->SetActorLocationAndRotation(NewLocation, GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);

	// 바이옴이 바뀌었으면 여기서 갈아 끼운다. 같은 바이옴이면 ApplyBiome 이 그냥 돌아온다.
	//
	// 앞쪽에서 재활용되는 청크부터 적용되므로, 전환은 화면 앞에서 시작해 시간을 두고
	// 기차 쪽으로 흘러온다. 이미 지나가고 있는 청크를 건드리지 않는 것이 요점이다 —
	// 눈앞에서 바위가 나무로 바뀌는 것을 막는다.
	Chunk->ApplyBiome(CurrentBiome, GetBiomeMeshDefinitions(CurrentBiome));

	const int32 SegmentIndex = NextSegmentIndex++;
	Chunk->GenerateChunk(MakeChunkSeed(SegmentIndex), SegmentIndex);
}

//매니저 위치 기준 청크의 상대 위치, 앞인지 뒤인지 확인
float ATRLevelSceneManager::GetTravelDistance(const ATRLevelSceneChunk* Chunk) const
{
	if (!IsValid(Chunk))
	{
		return -TNumericLimits<float>::Max();
	}

	return FVector::DotProduct(Chunk->GetActorLocation() - GetActorLocation(), GetActorForwardVector());
}

//가장 앞에 있는 청크까지의 거리 확인
float ATRLevelSceneManager::GetFrontmostTravelDistance(const ATRLevelSceneChunk* IgnoredChunk) const
{
	float FrontmostDistance = 0.f;
	bool bFoundChunk = false;
	for (const ATRLevelSceneChunk* Chunk : ActiveChunks)
	{
		if (!IsValid(Chunk) || Chunk == IgnoredChunk)
		{
			continue;
		}

		FrontmostDistance = bFoundChunk
			? FMath::Max(FrontmostDistance, GetTravelDistance(Chunk))
			: GetTravelDistance(Chunk);
		bFoundChunk = true;
	}

	return bFoundChunk ? FrontmostDistance : 0.f;
}

//기본 시드값 기반하여 청크 액터마다 가지는 개별 시드값 생성
int32 ATRLevelSceneManager::MakeChunkSeed(const int32 SegmentIndex) const
{
	return static_cast<int32>(HashCombine(GetTypeHash(RouteSeed), GetTypeHash(SegmentIndex)));
}

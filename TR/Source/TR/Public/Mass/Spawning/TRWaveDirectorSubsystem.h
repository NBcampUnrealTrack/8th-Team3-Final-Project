// TRWaveDirectorSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Mass/Spawning/TRMassSpawnUtils.h"
#include "Mass/Spawning/TRMassSpawnerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "TRWaveDirectorSubsystem.generated.h"

class AActor;

/** 기차 진행 방향 기준 스폰 방위. 기획서의 후/좌/우 3구역. */
UENUM(BlueprintType)
enum class ETRSpawnDirection : uint8
{
	Back,
	Left,
	Right,

	/**
	 * 좌우 양쪽에 절반씩. 기획서의 "협공" 패턴(대향 2방위 50/50)이다.
	 *
	 * 그룹을 둘로 나눠도 같은 결과를 낼 수 있지만, 한 그룹으로 두면 배치 수·간격을
	 * 한 곳에서만 고치면 되고 좌우 수가 어긋날 일이 없다.
	 */
	Sides UMETA(DisplayName = "Sides (Left + Right)"),

	All UMETA(DisplayName = "All (360)"),

	/**
	 * 좌 / 우 / 협공 중 하나를 무작위로 고른다. 후보 목록은 FTRWaveGroup::RandomDirections 다.
	 *
	 * **목록의 맨 뒤에 둔다.** 이 열거형은 DataTable 에 값으로 저장되므로 중간에 끼워 넣으면
	 * 뒤쪽 값이 전부 한 칸씩 밀려, 이미 저장된 웨이브 행의 방위가 조용히 바뀐다.
	 */
	Random UMETA(DisplayName = "Random (from candidates)"),
};

/**
 * 한 웨이브 안에서 **한 종류의 적**을 어떻게 낼지.
 *
 * 웨이브에는 종류가 섞여 나오고(기획서 W10 이후 근접형 + 사출형 + 사격형),
 * 종류마다 리듬이 완전히 다르다 — 근접형은 15초마다 17마리씩 여덟 번,
 * 사출형은 웨이브 전체에서 여덟 마리, 사격형은 셋. 이걸 하나의 배치 규칙으로
 * 표현할 수 없어서 종류별로 독립된 그룹으로 나눈다. 그룹마다 자기 타이머를 갖는다.
 */
USTRUCT(BlueprintType)
struct FTRWaveGroup
{
	GENERATED_BODY()

	/** 이 그룹이 스폰할 엔티티 Config. 종류·스탯은 Config의 TR Chaser 트레이트가 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group")
	TObjectPtr<UMassEntityConfigAsset> EnemyConfig = nullptr;

	/**
	 * 종류 표시. 로그와 검증에만 쓰고 실제 행동에는 영향을 주지 않는다
	 * (행동은 Config가 정한다). 테이블을 읽는 사람이 어느 줄이 뭔지 알아보라고 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group")
	ETREnemyType EnemyType = ETREnemyType::Melee;

	/** 한 배치에 몇 마리. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group", meta = (ClampMin = "1"))
	int32 BatchSize = 17;

	/** 배치 사이 간격(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float BatchInterval = 15.f;

	/** 배치를 몇 번 낼지. 0이면 웨이브가 끝날 때까지 무한히 반복한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group", meta = (ClampMin = "0"))
	int32 BatchCount = 8;

	/**
	 * 웨이브 시작 후 이 그룹의 첫 배치까지 대기(초).
	 *
	 * 종류를 시간차로 등장시키는 수단이다. 사격형이 근접형과 동시에 도착하면
	 * 플레이어가 어느 쪽을 볼지 고를 여유가 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float StartDelay = 0.f;

	/**
	 * 이 그룹의 총량 상한. 0이면 제한 없음(배치 수 × 배치 크기).
	 *
	 * 기획서의 "사출형 W3 = 2마리"처럼 총량으로 정의된 종류에 쓴다.
	 * 배치 크기를 1로 두고 이 값을 2로 주면 두 번 내고 끝난다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group", meta = (ClampMin = "0"))
	int32 TotalCount = 0;

	/** 이 그룹이 나올 방위. 그룹마다 다르게 주면 방향 패턴이 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group")
	ETRSpawnDirection Direction = ETRSpawnDirection::All;

	/** 스폰 거리(기차로부터). 기획서 기준 200m. 모양의 중심까지의 거리이기도 하다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SpawnDistance = 20000.f;

	/** 스폰 거리의 앞뒤 여유(고리의 두께). 한 줄로 서지 않게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SpawnDistanceJitter = 1500.f;

	/**
	 * 스폰 영역의 모양과 크기. 방위(Direction)가 정한 방향을 기준으로 배치되므로,
	 * 방위를 바꿔도 이 설정은 그대로 쓸 수 있다.
	 *
	 * 스폰 계산이 쓰는 구조체를 그대로 담는다 — 여기서 필드를 하나씩 옮겨 담으면
	 * 새 모양 옵션을 추가할 때 그 목록에 넣는 걸 잊어 웨이브에서만 무시되는 일이 생긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group")
	FTRSpawnShapeParams ShapeParams;

	/**
	 * 웨이브 스케일링(웨이브당 +5% 복리)을 이 그룹의 배치 크기에도 적용할지.
	 *
	 * 근접형은 물량이 늘어야 하지만, 사격형처럼 "웨이브당 +1마리"로 따로 정의된
	 * 종류는 여기서 함께 불리면 이중으로 늘어난다. 스탯 배수는 이 값과 무관하게
	 * 항상 적용된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group")
	bool bScaleBatchSize = true;

	/** 웨이브 번호마다 총량에 더할 수. 기획서의 "사격형 W10=3, 이후 웨이브당 +1"용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group")
	int32 TotalCountPerWave = 0;

	/**
	 * **종류 축** 배수. 이 그룹의 적에게만 추가로 걸린다.
	 * 웨이브 축 배수(FTRWaveDefinition::TierStatScale) 위에 곱해지며, 전부 1이면 아무 효과가 없다(기본값).
	 *
	 * 없으면 "같은 웨이브에서 근접형만 단단하게" 같은 조정을 하려고 Config 애셋을
	 * 복제해야 하는데, 그건 이 시스템이 애초에 피하려던 것이다 — 복제하면 밸런스를
	 * 고칠 때마다 전부 손봐야 하고 하나를 빠뜨리면 그 종류만 조용히 어긋난다.
	 *
	 * **웨이브 번호 복리(ScaleWaveForNumber)는 여기 걸리지 않는다.** 이 값은 "이 종류가
	 * 그 난이도에서 얼마나 단단한가"라는 고정 성격이고, 복리는 웨이브 배수 쪽 몫이다.
	 * 양쪽에 다 걸면 종류 간 격차가 웨이브마다 복리로 벌어져 후반에 손댈 수 없게 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group")
	FTREnemyStatScale TypeStatScale;

	/**
	 * 동시 상한에 막혀 못 낸 물량을 **몇 배치분까지 쌓아 뒀다가** 나중에 낼지.
	 *
	 * 예전에는 상한에 걸리면 그 배치를 그냥 소진시켰다. 그러면 밀리는 플레이어일수록
	 * 적이 덜 나오는 암묵적인 난이도 감면이 된다 — 성능 예산이 난이도 조절 장치로
	 * 새는 셈이라, 웨이브 총량이 기획서와 조용히 달라진다.
	 *
	 * 이제 못 낸 물량은 빚으로 남고 여유가 생기면 마저 나간다. 이 값은 그 빚의
	 * 상한이다. 없으면 오래 밀린 뒤 한숨 돌리는 순간 수백 마리가 쏟아져
	 * 프레임도 플레이어도 못 버틴다.
	 *
	 * **0이면 이월하지 않는다(예전 동작).** 2면 두 배치분까지 밀렸다가 나간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group", meta = (ClampMin = "0"))
	int32 MaxPendingBatches = 2;
};

/**
 * 한 웨이브의 스폰 규칙. 종류별 그룹의 묶음이다.
 *
 * 데이터 애셋이나 DataTable에 넣어 웨이브별로 관리하면 된다.
 */
USTRUCT(BlueprintType)
struct FTRWaveDefinition
{
	GENERATED_BODY()

	/** 이 웨이브에 나오는 종류들. 비어 있으면 아무것도 스폰되지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<FTRWaveGroup> Groups;

	/**
	 * 동시 생존 상한. 이 수를 넘지 않도록 배치 크기를 줄여서 스폰한다.
	 * 그룹별이 아니라 **웨이브 전체 기준**이다 — 성능 예산은 화면에 있는 총 개체 수로 정해진다.
	 *
	 * 물량이 늘어도 프레임이 무너지지 않게 하는 안전장치이며, 웨이브마다 다르게 줄 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "1"))
	int32 MaxConcurrent = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	ETRTargetChannel Channel = ETRTargetChannel::Train;

	/**
	 * **웨이브 축** 배수. T1 기준값에 곱해 그 웨이브의 실제 수치를 만든다.
	 *
	 * 배수가 두 축인 이유: 최종 배수는 (웨이브 축) x (종류 축)이다. 표로 치면 행 배수와
	 * 열 배수라, 하나로 합치면 웨이브 x 종류 조합마다 값을 따로 적어야 한다. 그러면 밸런스를
	 * 한 번 고칠 때 손대야 하는 칸이 곱으로 늘고, 하나를 빠뜨리면 그 조합만 조용히 어긋난다.
	 *
	 * 티어가 바뀌는 웨이브에만 행을 넣으면 된다 — 행이 없는 웨이브는 그보다 작은
	 * 최대 행을 물려받는다. 기획서 기준 1·4·7행이 T1·T2·T3 이고,
	 * W10 부터는 ScaleWaveForNumber 가 여기에 복리를 얹는다.
	 *
	 * 이 값은 웨이브 전체에 걸린다. 종류마다 다르게 주려면 FTRWaveGroup::TypeStatScale 을 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FTREnemyStatScale TierStatScale;

	/**
	 * 배치를 다 낸 뒤, 밀린 물량(MaxPendingBatches)을 마저 내보내려고 기다리는 최대 시간(초).
	 *
	 * 플레이어가 끝내 처리하지 못하면 빚이 영원히 안 빠지고 웨이브가 끝나지 않는다.
	 * 그러면 휴식도 다음 웨이브도 오지 않아 게임이 멈춘 것처럼 보인다. 그 상황을 막는
	 * 안전장치이며, 시간이 지나면 남은 빚을 버리고 웨이브를 마친다.
	 *
	 * 0이면 기다리지 않는다 — 배치를 다 내는 순간 빚을 버리고 끝낸다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float PendingDrainTime = 30.f;

	/**
	 * 방위가 Random 인 그룹들이 함께 뽑을 후보. 기본값은 좌 / 우 / 협공 셋이다.
	 *
	 * **웨이브 전체가 한 번 뽑아 같이 쓴다.** 그룹마다 따로 뽑으면 근접형은 왼쪽,
	 * 사격형은 오른쪽으로 갈려서 "이번엔 왼쪽에서 온다"는 판단 자체가 성립하지 않는다.
	 * 협공(Sides)이 뽑혔을 때도 종류가 흩어지면 협공으로 읽히지 않는다.
	 *
	 * 목록으로 둔 이유는 "뒤에서도 가끔 온다" 같은 조정을 코드 수정 없이 하기 위해서다.
	 * 같은 값을 두 번 넣으면 그만큼 자주 나온다 — 가중치 필드를 따로 두는 것보다
	 * 테이블에서 읽기 쉽다. Random 자신을 넣으면 무시된다.
	 *
	 * 비워 두면 협공으로 떨어지고 웨이브 시작 때 경고를 남긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<ETRSpawnDirection> RandomDirections = {
		ETRSpawnDirection::Left,
		ETRSpawnDirection::Right,
		ETRSpawnDirection::Sides
	};

	/**
	 * 웨이브 도중 방위를 다시 뽑는 주기(초). **0이면 웨이브당 한 번만 뽑는다(기본).**
	 *
	 * 0이면 그 웨이브 내내 방향이 고정이라 플레이어가 대응 전략을 세울 수 있다.
	 * 값을 주면 그 간격마다 방향이 바뀌어 사방을 계속 확인해야 한다 — 후반 웨이브의
	 * 난이도 장치로 쓸 수 있다.
	 *
	 * 그룹의 배치 간격이 아니라 **웨이브 시각 기준**이다. 그래야 서로 다른 타이머로
	 * 도는 그룹들이 같은 순간에 같은 방향을 본다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RandomDirectionInterval = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTROnWaveFinished);

/** 그룹 하나의 진행 상태. 그룹마다 타이머가 따로 돈다. */
USTRUCT()
struct FTRWaveGroupRuntime
{
	GENERATED_BODY()

	/** 남은 배치 수. 무한 그룹이면 의미 없음. */
	UPROPERTY()
	int32 BatchesRemaining = 0;

	/** 지금까지 이 그룹이 실제로 스폰한 수. TotalCount 상한 판정에 쓴다. */
	UPROPERTY()
	int32 SpawnedTotal = 0;

	/** 이 그룹의 총량 상한(웨이브 번호 보정이 반영된 값). 0이면 제한 없음. */
	UPROPERTY()
	int32 TotalLimit = 0;

	/**
	 * 스케줄상 나왔어야 하는데 동시 상한에 막혀 못 낸 물량.
	 *
	 * 이 값이 있어야 "배치를 냈다"와 "적이 실제로 나왔다"를 분리할 수 있다.
	 * 둘을 같은 것으로 두면 상한이 곧 난이도 감면 장치가 된다.
	 */
	UPROPERTY()
	int32 PendingSpawns = 0;

	/**
	 * 빚을 마저 내보내기를 포기할 월드 시각(초). 0이면 아직 정해지지 않음.
	 * 배치를 다 낸 시점에 한 번 정해진다.
	 */
	UPROPERTY()
	float DrainDeadline = 0.f;

	UPROPERTY()
	bool bFinished = false;

	FTimerHandle TimerHandle;
};

/**
 * 웨이브 진행을 관리한다. 정해진 간격으로 배치를 스폰하고, 동시 생존 수를 상한 안에 유지한다.
 *
 * 스폰 자체는 UTRMassSpawnerSubsystem이 하고, 이 서브시스템은 "언제 몇 마리를" 만 정한다.
 * 주의: 서버에서만 호출할 것.
 */
UCLASS(BlueprintType)
class TR_API UTRWaveDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 웨이브를 시작한다. 첫 배치는 즉시 나가고 이후 BatchInterval 간격으로 반복한다.
	 * 이미 진행 중이면 기존 웨이브를 중단하고 새로 시작한다.
	 *
	 * @param TargetActor 스폰 중심(보통 기차). 방위 계산의 기준 방향도 이 액터가 정한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Wave")
	void StartWave(const FTRWaveDefinition& WaveDefinition, AActor* TargetActor);

	/** 진행 중인 웨이브를 멈춘다. 이미 스폰된 적은 그대로 둔다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Wave")
	void StopWave();

	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	bool IsWaveActive() const { return bWaveActive; }

	/** 모든 그룹의 남은 배치 합계. 무한 그룹이 하나라도 있으면 -1. */
	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	int32 GetRemainingBatches() const;

	/** 현재 살아 있는 적 수. */
	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	int32 GetAliveCount() const;

	/**
	 * 웨이브 번호에 따라 물량과 스탯을 복리로 키운 사본을 돌려준다.
	 * 기획서의 "T4+ 스탯 웨이브당 +5% 복리"에 대응한다.
	 *
	 * 배치 크기는 bScaleBatchSize가 켜진 그룹만, 스탯 배수는 웨이브 전체에 적용된다.
	 * 그룹의 TotalCountPerWave는 복리가 아니라 웨이브당 정액으로 더해진다
	 * (기획서의 사격형 "웨이브당 +1마리"가 선형이기 때문).
	 *
	 * @param SourceWaveNumber  이 정의가 실제로 적힌 테이블 행의 웨이브 번호.
	 *                          TotalCountPerWave가 몇 번 더해질지를 여기서 센다.
	 * @param ScalingStartWave  이 웨이브부터 복리 스케일링을 적용한다.
	 */
	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	static FTRWaveDefinition ScaleWaveForNumber(const FTRWaveDefinition& BaseDefinition, int32 WaveNumber,
		int32 SourceWaveNumber, int32 ScalingStartWave = 10, float PerWaveMultiplier = 1.05f);

	/** 모든 배치를 다 내보냈을 때 발생. 무한 웨이브에서는 발생하지 않는다. */
	UPROPERTY(BlueprintAssignable, Category = "TR|Wave")
	FTROnWaveFinished OnWaveFinished;

protected:
	virtual void Deinitialize() override;

private:
	/**
	 * CurrentWave의 그룹마다 진행 상태를 초기화한다.
	 * @return 스폰 가능한(Config가 있는) 그룹 수. 0이면 웨이브를 시작할 수 없다.
	 */
	int32 InitializeGroupStates();

	/** 그룹마다 반복 타이머를 건다. */
	void ScheduleGroupTimers(UWorld& World);

	/** 지연이 없는 그룹의 첫 배치를 즉시 낸다. */
	void SpawnUndelayedGroups();

	/**
	 * 타이머가 부르는 그룹 1회 주기. 아래 세 단계를 순서대로 돌린다.
	 *
	 * 단계를 나눠 둔 이유는 **배치 스케줄과 실제 스폰이 다른 것**이기 때문이다.
	 * 둘을 한 덩어리로 두면 동시 상한에 막힌 물량이 조용히 사라져, 밀리는 플레이어일수록
	 * 적이 덜 나오는 난이도 감면이 된다.
	 */
	void SpawnGroupBatch(int32 GroupIndex);

	/**
	 * 1단계 — 이번 주기에 나왔어야 할 물량을 빚(PendingSpawns)에 더한다.
	 * 동시 상한과 무관하게 진행한다. 이게 웨이브의 리듬을 정하는 값이다.
	 */
	void AccrueScheduledBatch(int32 GroupIndex);

	/**
	 * 2단계 — 성능 예산(웨이브 전체의 동시 생존 상한)이 허락하는 만큼만 실제로 낸다.
	 * @return 실제로 스폰한 수.
	 */
	int32 SpawnPendingWithinBudget(int32 GroupIndex, UTRMassSpawnerSubsystem& Spawner);

	/**
	 * 3단계 — 이 그룹이 끝났는지 판정하고, 끝났으면 FinishGroup 을 부른다.
	 * @return 그룹을 종료시켰으면 true. 이 경우 호출자는 GroupStates 를 더 만지면 안 된다.
	 */
	bool EvaluateGroupCompletion(int32 GroupIndex);

	/** 그룹을 종료 처리하고, 모든 그룹이 끝났으면 웨이브를 끝낸다. */
	void FinishGroup(int32 GroupIndex);

	/**
	 * 그룹의 모양·방위 설정을 스폰 파라미터로 옮긴다.
	 *
	 * @param Direction 이 배치에 쓸 방위. Group.Direction 을 직접 보지 않는 이유는
	 *                  Random 일 때 실제 값이 런타임 상태(RolledDirection)에 있기 때문이다.
	 */
	void ResolveSpawnShape(const FTRWaveGroup& Group, ETRSpawnDirection Direction,
		FTRSpawnAreaParams& OutParams) const;

	/**
	 * 웨이브 공용 방위를 다시 뽑아야 할 때가 됐으면 뽑는다.
	 *
	 * 배치를 내기 직전마다 부른다. 주기가 0이면 웨이브 시작 시 한 번 뽑은 값을 계속 쓴다.
	 *
	 * @param bWarnOnEmpty 후보가 비었을 때 경고를 남길지. 웨이브 시작 때만 남기고
	 *                     배치마다 반복하지 않으려고 나눠 둔다.
	 */
	void UpdateRolledDirection(bool bWarnOnEmpty);

	/** 이 그룹이 이번 배치에 실제로 쓸 방위. Random 이면 웨이브 공용 값을 준다. */
	ETRSpawnDirection ResolveGroupDirection(const FTRWaveGroup& Group) const;

	UPROPERTY()
	FTRWaveDefinition CurrentWave;

	/** CurrentWave.Groups와 인덱스가 1:1로 대응한다. */
	UPROPERTY()
	TArray<FTRWaveGroupRuntime> GroupStates;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	/**
	 * 방위가 Random 인 그룹들이 **함께** 쓰는 값. 웨이브 단위로 뽑는다.
	 *
	 * 그룹 런타임이 아니라 여기 두는 것이 요점이다 — 그룹마다 들고 있으면 각자 다른
	 * 값을 갖게 되고, 그러면 웨이브 전체가 한 방향에서 온다는 성질이 깨진다.
	 */
	ETRSpawnDirection RolledDirection = ETRSpawnDirection::Sides;

	/** 다음으로 방위를 다시 뽑을 월드 시각(초). 주기가 0이면 쓰이지 않는다. */
	float NextDirectionRollTime = 0.f;

	/** 웨이브 종료 알림을 다음 틱으로 미룬 타이머. StopWave 에서 지운다. */
	FTimerHandle WaveFinishedTimerHandle;

	bool bWaveActive = false;
};

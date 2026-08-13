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
	 * 스탯 배수. ScaleWaveForNumber가 채워 넣는 값이라 테이블에서 직접 건드리지 않는다.
	 * 스폰 시 체력·피해량에 곱해진다(기획서의 "웨이브당 +5% 복리").
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave")
	float StatScale = 1.f;
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

	/** 타이머가 부르는 그룹 1회 배치 스폰. */
	void SpawnGroupBatch(int32 GroupIndex);

	/** 그룹을 종료 처리하고, 모든 그룹이 끝났으면 웨이브를 끝낸다. */
	void FinishGroup(int32 GroupIndex);

	/** 그룹의 모양·방위 설정을 스폰 파라미터로 옮긴다. */
	void ResolveSpawnShape(const FTRWaveGroup& Group, FTRSpawnAreaParams& OutParams) const;

	UPROPERTY()
	FTRWaveDefinition CurrentWave;

	/** CurrentWave.Groups와 인덱스가 1:1로 대응한다. */
	UPROPERTY()
	TArray<FTRWaveGroupRuntime> GroupStates;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	bool bWaveActive = false;
};

// TRWaveSpawner.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Mass/Spawning/TRWaveDirectorSubsystem.h"
#include "TRWaveSpawner.generated.h"

class UDataTable;

/**
 * 웨이브 테이블의 한 행. RowName = 웨이브 번호 ("1", "2", ...).
 *
 * 프로젝트의 다른 테이블(FTRArmorLevelRow 등)은 TRTypes.h에 모여 있지만,
 * 이 행은 Mass 타입(Config 애셋, 채널)을 참조하므로 여기에 둔다.
 * TRTypes.h에 넣으면 Mass 헤더가 프로젝트 전역으로 딸려간다.
 */
USTRUCT(BlueprintType)
struct FTRWaveRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 웨이브의 스폰 규칙. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FTRWaveDefinition Wave;

	/** 이 웨이브가 끝나고 다음 웨이브까지의 휴식(초). 기획서 기준 40초. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RestDuration = 40.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTROnWaveNumberChanged, int32, WaveNumber);

/**
 * 웨이브 테이블을 읽어 웨이브를 순서대로 진행시키는 레벨 액터.
 *
 * 역할 분담:
 *   ATRWaveSpawner        어느 웨이브인가, 테이블에서 규칙을 어떻게 읽는가, 언제 다음으로 넘어가는가
 *   UTRWaveDirectorSubsystem  한 웨이브 안에서 배치를 언제 몇 마리 낼 것인가
 *   UTRMassSpawnerSubsystem   실제 엔티티 생성
 *
 * 테이블에 없는 웨이브 번호는 **마지막 행을 복리로 키워서** 사용한다.
 * 기획서의 "T4+ 웨이브당 +5% 복리"가 이것으로 표현되며, 덕분에 테이블에
 * 웨이브를 무한히 나열할 필요가 없다.
 */
UCLASS()
class TR_API ATRWaveSpawner : public AActor
{
	GENERATED_BODY()

public:
	ATRWaveSpawner();

	/** 첫 웨이브부터 진행을 시작한다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Wave")
	void StartWaves();

	/** 진행을 멈춘다. 이미 스폰된 적은 그대로 둔다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Wave")
	void StopWaves();

	/** 특정 웨이브로 건너뛴다. 테스트용으로 유용하다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Wave")
	void StartWaveNumber(int32 WaveNumber);

	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	int32 GetCurrentWaveNumber() const { return CurrentWaveNumber; }

	/** 휴식 중인지. */
	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	bool IsResting() const;

	UPROPERTY(BlueprintAssignable, Category = "TR|Wave")
	FTROnWaveNumberChanged OnWaveStarted;

	/** 배치를 다 내보낸 시점. 적이 모두 죽은 시점이 아니다. */
	UPROPERTY(BlueprintAssignable, Category = "TR|Wave")
	FTROnWaveNumberChanged OnWaveCleared;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** RowStruct = FTRWaveRow. RowName은 웨이브 번호. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Wave")
	TObjectPtr<UDataTable> WaveTable;

	/**
	 * 스폰 중심이자 방위 기준이며, 동시에 **적들이 추적할 목표**다.
	 * 비워두면 BeginPlay에서 ATRTrain을 찾는다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "TR|Wave")
	TObjectPtr<AActor> TargetActor;

	/**
	 * 위 액터를 TargetDirector에 자동 등록한다.
	 *
	 * 이걸 끄면 블루프린트에서 직접 SetTargetActor를 호출해야 한다.
	 * 켜 두면 "목표가 누구인가"를 이 액터 한 곳에서만 정하면 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Wave")
	bool bAutoRegisterTarget = true;

	/** 자동 등록할 채널. 웨이브 테이블의 Channel과 같아야 적이 이 목표를 쫓는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Wave")
	ETRTargetChannel TargetChannel = ETRTargetChannel::Train;

	// 충돌 영역의 크기는 여기서 정하지 않는다. 기차 액터의 TargetVolume 컴포넌트를
	// 뷰포트에서 직접 조절하는 것이 유일한 저작 경로다. 두 군데서 정하면 어긋난다.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Wave")
	bool bAutoStartOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Wave", meta = (ClampMin = "1"))
	int32 FirstWaveNumber = 1;

	/** 테이블에 없는 웨이브에 적용할 스케일링. 기획서 기준 W10부터 웨이브당 +5%. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Wave|Scaling", meta = (ClampMin = "1"))
	int32 ScalingStartWave = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Wave|Scaling", meta = (ClampMin = "1.0"))
	float PerWaveMultiplier = 1.05f;

private:
	/** 웨이브 번호에 해당하는 행을 찾는다. 없으면 그보다 작은 번호 중 가장 큰 행. */
	const FTRWaveRow* FindWaveRow(int32 WaveNumber, int32& OutSourceWaveNumber) const;

	/** 디렉터의 웨이브 종료 알림. */
	UFUNCTION()
	void HandleWaveFinished();

	/** 휴식 종료 후 다음 웨이브. */
	void AdvanceToNextWave();

	UPROPERTY()
	int32 CurrentWaveNumber = 0;

	FTimerHandle RestTimerHandle;

	bool bRunning = false;
};

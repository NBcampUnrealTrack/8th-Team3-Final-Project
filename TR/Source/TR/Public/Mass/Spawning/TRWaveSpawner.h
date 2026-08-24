// TRWaveSpawner.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Mass/Spawning/TRWaveDirectorSubsystem.h"
#include "TRTypes.h"
#include "TRWaveSpawner.generated.h"

class ATRGameState;
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

	/**
	 * 이 웨이브의 풍경. 드롭다운에서 고른다.
	 *
	 * **None 이면 직전 풍경을 그대로 유지한다.** 그래서 바뀌는 웨이브에만 지정하면 되고,
	 * 이미 만들어 둔 행을 손댈 필요가 없다.
	 *
	 * 웨이브 번호를 두 곳에 적지 않으려고 여기에 둔다. 풍경 쪽에도 시작 웨이브를
	 * 적어 두면 한쪽만 고쳤을 때 조용히 어긋나는데, 그때 화면에 나오는 것은
	 * "웨이브는 넘어갔는데 풍경만 그대로"라 원인을 찾기 어렵다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	ETRBiomeType Biome = ETRBiomeType::None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTROnWaveNumberChanged, int32, WaveNumber);

/** @param Duration 이 휴식의 총 길이(초). 남은 시간은 GetRestTimeRemaining 으로 잰다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTROnRestStarted, float, Duration);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTROnRestEnded);

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

	/**
	 * 휴식 중인지.
	 *
	 * **서버 전용 판정이다.** 이 액터는 복제되지 않아 클라이언트에는 존재하지 않는다.
	 * UI 를 비롯한 클라이언트 코드는 ATRGameState::IsResting 을 봐야 한다.
	 */
	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	bool IsResting() const;

	/** 휴식이 끝나기까지 남은 시간(초). 휴식 중이 아니면 0. 서버 전용. */
	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	float GetRestTimeRemaining() const;

	/**
	 * 남은 휴식을 건너뛰고 다음 웨이브를 곧바로 시작한다.
	 *
	 * "준비 완료" 버튼처럼 플레이어가 판을 앞당기는 경로를 위해 둔다.
	 * 휴식 중이 아니면 아무것도 하지 않고 false.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Wave")
	bool SkipRest();

	UPROPERTY(BlueprintAssignable, Category = "TR|Wave")
	FTROnWaveNumberChanged OnWaveStarted;

	/** 배치를 다 내보낸 시점. 적이 모두 죽은 시점이 아니다. */
	UPROPERTY(BlueprintAssignable, Category = "TR|Wave")
	FTROnWaveNumberChanged OnWaveCleared;

	/** 휴식이 시작된 시점. 업그레이드 UI 를 여는 신호로 쓴다. */
	UPROPERTY(BlueprintAssignable, Category = "TR|Wave")
	FTROnRestStarted OnRestStarted;

	/** 휴식이 끝난 시점. 다음 웨이브의 OnWaveStarted 직전에 발화한다. */
	UPROPERTY(BlueprintAssignable, Category = "TR|Wave")
	FTROnRestEnded OnRestEnded;

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

	/**
	 * 살아 있는 적 수를 GameState 에 밀어 넣는 주기(초).
	 *
	 * 매 프레임 셀 필요가 없는 값이다 — 세는 쪽이 핸들 배열을 훑고, 받는 쪽은 UI 라
	 * 사람 눈에 0.5초 단위면 충분하다. 짧게 줄이면 복제량과 순회 비용만 늘어난다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Wave", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float AliveCountPushInterval = 0.5f;

private:
	/** 웨이브 번호에 해당하는 행을 찾는다. 없으면 그보다 작은 번호 중 가장 큰 행. */
	const FTRWaveRow* FindWaveRow(int32 WaveNumber, int32& OutSourceWaveNumber) const;

	/**
	 * 테이블 행을 이번 웨이브에 실제로 쓸 정의로 만든다.
	 * 행 번호가 웨이브 번호와 다르면(= 테이블에 없는 웨이브) 복리로 키운 사본을 준다.
	 */
	FTRWaveDefinition ResolveWaveDefinition(const FTRWaveRow& Row, int32 WaveNumber, int32 SourceWaveNumber) const;

	/** 두 축 배수를 로그로 남긴다. 테이블에 적은 값이 왜 안 먹는지 추적하는 유일한 단서다. */
	void LogWaveStatScales(const FTRWaveDefinition& Definition, int32 WaveNumber) const;

	/** 이 웨이브의 채널에 추적 목표가 등록돼 있는지 확인하고, 없으면 로그로 알린다. */
	void VerifyWaveTarget(const FTRWaveDefinition& Definition, int32 WaveNumber) const;

	/** 디렉터의 웨이브 종료 알림. */
	UFUNCTION()
	void HandleWaveFinished();

	/** 휴식 종료 후 다음 웨이브. */
	void AdvanceToNextWave();

	/**
	 * 클라이언트가 볼 수 있는 유일한 창구에 상태를 밀어 넣는다.
	 * 이 액터는 복제되지 않으므로, 여기를 거치지 않은 값은 서버 밖으로 나가지 않는다.
	 */
	ATRGameState* GetTRGameState() const;

	/** @param RestDuration Rest 단계일 때만 쓴다. 끝나는 시각으로 환산해 복제한다. */
	void PushWavePhase(ETRWavePhase Phase, float RestDuration = 0.f);

	/** 반복 타이머가 부른다. 살아 있는 적 수를 GameState 에 복제한다. */
	void PushAliveCount();

	/**
	 * 그 웨이브 행의 풍경을 GameState 에 밀어 넣는다. None 이면 아무것도 하지 않는다.
	 *
	 * **테이블을 읽는 곳을 여기 하나로 유지하려고 둔다.** 레벨 매니저가 테이블을 직접
	 * 읽게 하면 행이 없을 때의 승계 규칙을 두 곳에서 구현해야 하고, 두 참조가 서로 다른
	 * 테이블을 가리키는 사고도 생긴다.
	 */
	void PushBiomeForWave(int32 WaveNumber);

	UPROPERTY()
	int32 CurrentWaveNumber = 0;

	/**
	 * 지금 웨이브가 끝나면 쉴 시간(초). 웨이브를 **시작할 때** 행에서 읽어 둔다.
	 *
	 * 끝나는 시점에 테이블을 다시 뒤지지 않기 위해서이기도 하지만, 그보다
	 * 스폰 규칙과 휴식 시간이 같은 행의 같은 스냅샷에서 나와야 하기 때문이다.
	 * 양쪽을 다른 시점에 읽으면 진행 중 테이블이 바뀌었을 때 서로 다른 행을 볼 수 있다.
	 */
	UPROPERTY()
	float CurrentRestDuration = 0.f;

	FTimerHandle RestTimerHandle;

	FTimerHandle AliveCountTimerHandle;

	bool bRunning = false;
};

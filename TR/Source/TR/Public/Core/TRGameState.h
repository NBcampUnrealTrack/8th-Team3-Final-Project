// TRGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TRTypes.h"
#include "TRGameState.generated.h"

class UDataTable;
class UTRActionComponent;
struct FTRArmorLevelRow;
struct FTRTurretLevelRow;

// 파츠 업그레이드 시 CurrentHP의 비율 유지를 위한 델리게이트
DECLARE_MULTICAST_DELEGATE_OneParam(FTRPartLevelUpgraded, float /*PrevMaxHP*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FTRMatchStartedChanged, bool /*bStarted*/);

// 웨이브 상태 알림. 서버·클라이언트 양쪽에서 값이 바뀐 직후 발화한다.
// UI 가 매 프레임 값을 폴링하지 않아도 되게 하려고 둔다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTROnWaveNumberReplicated, int32, WaveNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTROnWavePhaseReplicated, ETRWavePhase, Phase);

UCLASS()
class TR_API ATRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	int32 GetMonsterResource() const { return MonsterResource; }

	// 검사와 차감을 함께 한다. 부족하면 아무것도 깎지 않고 false.
	bool TryConsumeMonsterResource(int32 Amount);
	
	void AddMonsterResource(int32 Amount);
	
	int32 GetArmorLevel() const { return ArmorLevel; }
	int32 GetTurretLevel() const { return TurretLevel; }

	// 레벨별 수치 조회
	const FTRArmorLevelRow* GetArmorLevelRow(const TCHAR* ContextName) const;
	const FTRTurretLevelRow* GetTurretLevelRow(const TCHAR* ContextName) const;

	// 현재 레벨에서 다음 레벨로 올리는 비용
	int32 GetArmorUpgradeCost() const;
	int32 GetTurretUpgradeCost() const;

	// 비용 검사·차감·레벨 상승을 함께 한다. 하나라도 실패하면 아무것도 바뀌지 않는다.
	bool TryUpgradeArmorLevel();
	bool TryUpgradeTurretLevel();

	// 서버에서 레벨이 오른 직후 1회. 파츠가 새 MaxHP에 맞춰 자기 HP 비율을 유지하는 데 사용.
	// 인자는 오르기 전 최대 HP
	FTRPartLevelUpgraded OnArmorLevelUpgraded;
	FTRPartLevelUpgraded OnTurretLevelUpgraded;

	FTRMatchStartedChanged OnMatchStartedChanged;

	static constexpr int32 MaxArmorLevel = 4;
	static constexpr int32 MaxTurretLevel = 3;

	// 재설치는 열차 전체에서 한 번에 하나만 돈다. 진행 중인 컴포넌트, 없으면 nullptr.
	// 소유 액터가 사라지면 저절로 nullptr 이 된다.
	UTRActionComponent* GetBuildingComponent() const;

	// 서버 전용. 갱신 지점을 하나로 두려고 UTRActionComponent 가 자기 상태를 바꿀 때만 부른다.
	void SetBuildingComponent(UTRActionComponent* InComponent);

	/** 웨이브가 시작됐는지. 로비 UI 를 닫는 신호로도 쓴다. */
	UFUNCTION(BlueprintPure, Category = "TR|Lobby")
	bool IsMatchStarted() const { return bMatchStarted; }

	/** 준비를 누른 인원. 호스트는 세지 않는다. */
	UFUNCTION(BlueprintPure, Category = "TR|Lobby")
	int32 GetReadyPlayerCount() const;

	/** 시작에 필요한 인원. 호스트를 뺀 접속자 수다. */
	UFUNCTION(BlueprintPure, Category = "TR|Lobby")
	int32 GetRequiredReadyCount() const;

	/** 호스트가 시작을 누를 수 있는 상태인지. 혼자면 바로 시작할 수 있다. */
	UFUNCTION(BlueprintPure, Category = "TR|Lobby")
	bool CanStartMatch() const;

	/** 서버에서만 부른다. */
	void SetMatchStarted(bool bInStarted);

	// ── 웨이브 상태 ─────────────────────────────────────────────────────────
	//
	// 웨이브 진행은 서버의 ATRWaveSpawner 가 정하고, **결과만** 여기에 복제된다.
	// UI 가 스포너를 직접 찾지 않는 이유는 스포너가 bReplicates=false 라
	// 클라이언트 월드에는 아예 존재하지 않기 때문이다. 상태를 보는 창구는 여기 하나다.

	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	int32 GetWaveNumber() const { return WaveNumber; }

	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	ETRWavePhase GetWavePhase() const { return WavePhase; }

	/** 휴식 중인지. 업그레이드·재설치처럼 휴식에만 허용할 행동의 판정에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	bool IsResting() const { return WavePhase == ETRWavePhase::Rest; }

	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	bool IsGameOver() const { return WavePhase == ETRWavePhase::Over; }

	/**
	 * 휴식이 끝나기까지 남은 시간(초). 휴식 중이 아니면 0.
	 *
	 * 남은 시간이 아니라 **끝나는 시각**을 복제하므로 클라이언트에서도 값이 맞는다.
	 * 남은 시간을 복제하면 복제 주기 사이에 카운트다운이 멈춰 보인다.
	 */
	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	float GetRestTimeRemaining() const;

	/** 살아 있는 적 수. 서버가 주기적으로 갱신한다(시체는 세지 않는다). */
	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	int32 GetAliveEnemyCount() const { return AliveEnemyCount; }

	/**
	 * 지금 진행 중이거나 곧 시작할 웨이브의 풍경.
	 *
	 * 휴식에 들어가는 순간 **다음 웨이브의 값**으로 바뀐다. 그래야 배경이 휴식 동안
	 * 미리 흘러 들어와 전투가 시작될 때는 이미 바뀌어 있다.
	 *
	 * 값의 출처는 웨이브 테이블(FTRWaveRow::Biome)이고, 서버의 ATRWaveSpawner 만
	 * 갱신한다. 클라이언트가 테이블을 직접 읽지 않는 이유는 행 승계 규칙을 두 곳에서
	 * 구현하지 않기 위해서다.
	 */
	UFUNCTION(BlueprintPure, Category = "TR|Wave")
	ETRBiomeType GetBiome() const { return Biome; }

	// 아래 셋은 서버 전용 — ATRWaveSpawner 와 ATRGameMode 가 유일한 갱신 지점이다.

	void SetWaveNumber(int32 InWaveNumber);

	/** @param InRestEndServerTime 휴식이 끝나는 서버 월드 시각. Rest 단계에서만 의미가 있다. */
	void SetWavePhase(ETRWavePhase InPhase, float InRestEndServerTime = 0.f);

	void SetAliveEnemyCount(int32 InCount);

	void SetBiome(ETRBiomeType InBiome);

	UPROPERTY(BlueprintAssignable, Category = "TR|Wave")
	FTROnWaveNumberReplicated OnWaveNumberChanged;

	UPROPERTY(BlueprintAssignable, Category = "TR|Wave")
	FTROnWavePhaseReplicated OnWavePhaseChanged;

protected:
	virtual void BeginPlay() override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void HandleEnemyKilled(const FVector& Location, int32 ResourceValue);

	UFUNCTION()
	void OnRep_MatchStarted();

	/** 자원을 차감하고 레벨을 올린 뒤 알린다. 장갑·포탑 공용. */
	bool ConsumeAndUpgradePart(int32 Cost, float PrevMaxHP, int32& Level, FTRPartLevelUpgraded& OnUpgraded);
	
	UPROPERTY(Replicated)
	int32 MonsterResource = 0;
	
	UPROPERTY(Replicated)
	int32 ArmorLevel = 1;

	UPROPERTY(Replicated)
	int32 TurretLevel = 1;

	UPROPERTY(Replicated)
	TObjectPtr<UTRActionComponent> BuildingComponent;

	// 파츠 레벨별 수치. EditDefaultsOnly 라 복제 없이 클라이언트도 같은 값을 갖는다.
	UPROPERTY(EditDefaultsOnly, Category = "Level")
	TObjectPtr<UDataTable> ArmorLevelTable;    // FTRArmorLevelRow, RowName = 레벨

	UPROPERTY(EditDefaultsOnly, Category = "Level")
	TObjectPtr<UDataTable> TurretLevelTable;   // FTRTurretLevelRow, RowName = 레벨

	UPROPERTY(ReplicatedUsing = OnRep_MatchStarted)
	bool bMatchStarted = false;

	// ── 웨이브 상태(복제) ───────────────────────────────────────────────────
	//
	// OnRep 은 클라이언트에서만 발화하므로, 서버 쪽 세터가 직접 한 번 더 부른다.
	// 그러지 않으면 리슨 서버의 호스트만 UI 가 갱신되지 않는다.

	UPROPERTY(ReplicatedUsing = OnRep_WaveNumber)
	int32 WaveNumber = 0;

	UPROPERTY(ReplicatedUsing = OnRep_WavePhase)
	ETRWavePhase WavePhase = ETRWavePhase::None;

	/**
	 * 휴식이 끝나는 **서버** 월드 시각(초).
	 * AGameStateBase 가 서버 시간을 이미 복제하므로, 클라이언트도 같은 축으로 잰다.
	 */
	UPROPERTY(Replicated)
	float RestEndServerTime = 0.f;

	UPROPERTY(Replicated)
	int32 AliveEnemyCount = 0;

	/** 배경 풍경. ATRLevelSceneManager 가 이 값으로 자기 Biomes 맵에서 고른다. */
	UPROPERTY(Replicated)
	ETRBiomeType Biome = ETRBiomeType::None;

	UFUNCTION()
	void OnRep_WaveNumber();

	UFUNCTION()
	void OnRep_WavePhase();
};

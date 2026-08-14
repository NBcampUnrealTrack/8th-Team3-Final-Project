// TRGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TRGameState.generated.h"

class UDataTable;
class UTRActionComponent;
struct FTRArmorLevelRow;
struct FTRTurretLevelRow;

// 파츠 업그레이드 시 CurrentHP의 비율 유지를 위한 델리게이트
DECLARE_MULTICAST_DELEGATE_OneParam(FTRPartLevelUpgraded, float /*PrevMaxHP*/);

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

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
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

	UPROPERTY(Replicated)
	bool bMatchStarted = false;
};

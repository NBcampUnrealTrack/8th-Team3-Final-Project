// TRGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TRGameState.generated.h"


UCLASS()
class TR_API ATRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	int32 GetMonsterResource() const { return MonsterResource; }

	// 검사와 차감을 함께 한다. 부족하면 아무것도 깎지 않고 false.
	bool TryConsumeMonsterResource(int32 Amount);
	
	int32 GetArmorLevel() const { return ArmorLevel; }
	int32 GetTurretLevel() const { return TurretLevel; }
	
	bool UpgradeArmorLevel();
	bool UpgradeTurretLevel();

	static constexpr int32 MaxArmorLevel = 4;
	static constexpr int32 MaxTurretLevel = 3;

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
	bool bMatchStarted = false;
};

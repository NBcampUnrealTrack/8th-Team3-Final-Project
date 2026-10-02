// BGAIBrainComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BGAIBrainComponent.generated.h"


class ABGAIPlayerController;
class ABGGameState;
class UBGDataSubsystem;
struct FBGCardInstance;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BG_API UBGAIBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGAIBrainComponent();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	// GameState::OnTurnStateChanged 구독 콜백.
	UFUNCTION()
	void HandleTurnStateChanged();

	void TryPlayNextCard();

	// 규칙 1,2,3 검사해서 카드 선택
	bool SelectCardToPlay(int32& OutInstanceId, int32& OutTargetSeat) const;

	// 카드 한 장을 회복/공격/대상필요 여부로 분류한다.
	void ClassifyCard(const UBGDataSubsystem* DataSys, const FBGCardInstance& Card,
		bool& bOutHeal, bool& bOutAttack, bool& bOutNeedsTarget) const;

	// 공격 카드 대상 선정 (지금은 무작위)
	int32 PickRandomTargetSeat() const;

	// RoleTag 가 보안관 편(보안관·부관)인지 판정한다. PvE 팀 구분에 쓴다(배신자는 PvE에 없음).
	bool IsSheriffSideRole(FGameplayTag RoleTag) const;

	ABGAIPlayerController* GetOwnerAIController() const;
	ABGGameState* GetBGGameState() const;

	// 카드 한 장을 낸 뒤 다음 판단까지의 최소 대기시간(초)
	UPROPERTY(EditDefaultsOnly, Category="BG|AI")
	float MinActionInterval = 1.5f;

	// 연출이 끝나(카드를 낼 수 있게 되)고 이만큼 기다린 뒤 다음 카드를 낸다.
	// 서버 연출 뒤에 남는 화면 꼬리(게이지 결과·HUD 복귀)를 덮지 않기 위해서다.
	UPROPERTY(EditDefaultsOnly, Category="BG|AI", meta=(ClampMin="0"))
	float PostPresentationDelay = 1.2f;

	// 연출이 끝나기를 기다리며 다시 보는 주기(초). 짧아야 열린 순간을 놓치지 않는다.
	UPROPERTY(EditDefaultsOnly, Category="BG|AI", meta=(ClampMin="0.05"))
	float GatePollInterval = 0.2f;

	// 카드를 낼 수 있게 된 시각. -1 이면 아직 닫혀 있다.
	double GateOpenedTime = -1.0;

	FTimerHandle ActionTimerHandle;

	// 이번 턴에 시도했다가 거부된 InstanceId. HandleTurnStateChanged 가 내 턴 진입 시 비운다.
	TSet<int32> RejectedThisTurn;
};

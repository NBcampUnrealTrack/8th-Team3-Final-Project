// BGAIBrainComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

	ABGAIPlayerController* GetOwnerAIController() const;
	ABGGameState* GetBGGameState() const;

	// 카드 한 장을 낸 뒤 다음 판단까지의 최소 대기시간(초)
	UPROPERTY(EditDefaultsOnly, Category="BG|AI")
	float MinActionInterval = 1.5f;

	FTimerHandle ActionTimerHandle;

	// 이번 턴에 시도했다가 거부된 InstanceId. HandleTurnStateChanged 가 내 턴 진입 시 비운다.
	TSet<int32> RejectedThisTurn;
};

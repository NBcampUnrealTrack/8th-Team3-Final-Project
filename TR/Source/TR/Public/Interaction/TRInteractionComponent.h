// TRInteractionComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TRInteractionComponent.generated.h"

class UTRPlayerHUDWidget;
class UTRActionComponent;
enum class ETRActionType : uint8;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TR_API UTRInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UTRInteractionComponent();

	// 현재 라인 트레이스가 잡고 있는 대상. 없으면 nullptr.
	UFUNCTION(BlueprintPure, Category="Interaction")
	AActor* GetCurrentTarget() const { return LastTraceResult.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void UpdateTrace();

	void UpdateInteractionWidget();

	// HUD 게이지가 읽을 진행률. 수리는 대상이 사이클을 가지므로 출처가 다르다.
	float ResolveActionProgress01(const UTRActionComponent& ActionComp, ETRActionType ActionType) const;

	// 로컬 플레이어의 HUD 위젯. 서버/원격 클라이언트에서는 nullptr.
	UTRPlayerHUDWidget* GetHUDWidget() const;

	// 이건 매번 새로 계산한 결과를 다음 조회 전까지만 들고 있는 캐시
	TWeakObjectPtr<AActor> LastTraceResult;

	UPROPERTY(EditDefaultsOnly, Category="Interaction")
	float TraceDistance = 200.f;
};

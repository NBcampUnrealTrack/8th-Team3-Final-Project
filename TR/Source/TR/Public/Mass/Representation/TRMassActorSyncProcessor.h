// TRMassActorSyncProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRMassActorSyncProcessor.generated.h"

/**
 * Mass 엔티티의 트랜스폼을 표현 액터에 밀어넣는다.
 *
 * 표현 시스템은 액터를 스폰할 때 위치를 한 번 정해줄 뿐, 이후 추적은 하지 않는다.
 * 이게 없으면 액터가 스폰된 자리에 그대로 서 있고 엔티티만 움직인다.
 *
 * 엔진에도 번역기(UMassSceneComponentLocationToActorTranslator 등)가 있지만
 * 액터에 UMassAgentComponent를 붙이고 래퍼 프래그먼트를 채우는 경로를 전제한다.
 * 우리 구조는 Mass가 트랜스폼의 단독 주인이라 그 왕복이 필요 없다.
 */
UCLASS()
class TR_API UTRMassActorSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRMassActorSyncProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

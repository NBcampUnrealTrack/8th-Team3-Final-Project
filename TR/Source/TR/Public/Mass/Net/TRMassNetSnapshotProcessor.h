// TRMassNetSnapshotProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRMassNetSnapshotProcessor.generated.h"

class ATRMassNetRelay;

/**
 * 서버의 Mass 적을 압축 스냅샷으로 모아 복제 릴레이에 넘긴다.
 *
 * 클라이언트에는 Mass 시뮬레이션이 없다(엔진이 StateTree 를 서버 전용으로 제한한다).
 * 그래서 "무엇이 어디 있는가"만 주기적으로 보내고, 클라이언트는 그걸로 표현만 만든다.
 * 자세한 배경은 ATRMassNetRelay 주석 참고.
 *
 * 갱신 주기는 릴레이가 정한다. 매 프레임 보내면 대역폭이 프레임레이트에 끌려가는데,
 * 표현용이라 그럴 이유가 없다.
 */
UCLASS()
class TR_API UTRMassNetSnapshotProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRMassNetSnapshotProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	/** 다음 스냅샷 시각(월드 초). */
	float NextSnapshotTime = 0.f;

	/** 매 프레임 찾지 않도록 들고 있는다. */
	TWeakObjectPtr<ATRMassNetRelay> CachedRelay;
};

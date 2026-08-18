// TRMassDeathProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRMassDeathProcessor.generated.h"

/**
 * 사망 연출이 끝난 시체를 지운다.
 *
 * 죽이는 쪽(포탑 사격, 탄 소진 등)은 상태만 Death 로 바꾸고 FTRDeadTag 를 붙인다.
 * **삭제 시점은 전부 여기서 정한다.** 죽는 경로마다 각자 지우게 두면 연출 시간이 제각각이 되고,
 * 두 경로가 겹칠 때 같은 엔티티를 두 번 지우려 든다.
 *
 * 시체는 조종·분리 프로세서에서 제외되므로(FTRDeadTag) 이 시간 동안 그 자리에 멈춰 있고,
 * 표현 쪽은 ActionState 가 Death 라 사망 클립을 재생한다.
 */
UCLASS()
class TR_API UTRMassDeathProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRMassDeathProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

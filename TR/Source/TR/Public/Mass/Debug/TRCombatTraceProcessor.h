// TRCombatTraceProcessor.h
//
// 임시 계측용. 지울 때는 이 파일과 짝 cpp 만 지우면 된다.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "Mass/Core/TRMassTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TRCombatTraceProcessor.generated.h"

struct FTRMassImpactEvent;

/**
 * 기차가 받은 피해를 원인(SourceType)과 함께 찍는다.
 * 명령: tr.Trace.Combat 1
 */
UCLASS()
class TR_API UTRCombatTraceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	UFUNCTION()
	void HandleImpact(const FTRMassImpactEvent& Impact);

	UFUNCTION()
	void HandleKilled(const FVector& Location, int32 ResourceValue);
};

/** 적 개체의 사격 수·상태 전이·표적 침투를 찍는다. 값은 바꾸지 않는다. */
UCLASS()
class TR_API UTRCombatTraceProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRCombatTraceProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	/** 직전 프레임에 본 값. 바뀐 순간만 찍기 위해 들고 있는다. */
	struct FSeen
	{
		int32 ShotsFired = 0;
		ETREnemyActionState ActionState = ETREnemyActionState::None;
		bool bInsideHull = false;
		double ArrivedSince = 0.0;
		bool bReportedStuck = false;
	};

	TMap<FMassEntityHandle, FSeen> LastSeen;

	FMassEntityQuery TraceQuery;
};

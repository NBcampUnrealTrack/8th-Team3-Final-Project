// TRMassCombatSubsystem.cpp

#include "Mass/Combat/TRMassCombatSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassNavigationFragments.h"
#include "MassNavigationSubsystem.h"
#include "Mass/Core/TRMassParams.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Targeting/TRTargetDirectorSubsystem.h"
#include "Train/TRTrain.h"

void UTRMassCombatSubsystem::Deinitialize()
{
	PendingImpacts.Empty();

	Super::Deinitialize();
}

void UTRMassCombatSubsystem::EnqueueImpact(const FTRMassImpactEvent& Impact)
{
	PendingImpacts.Enqueue(Impact);
}

FTRMassHitResult UTRMassCombatSubsystem::FireHitscan(const FVector& Start, const FVector& Direction,
	float Range, float Damage, float TraceRadius)
{
	FTRMassHitResult Result;

	if (!ensureMsgf(IsInGameThread(), TEXT("FireHitscan 은 게임 스레드에서만 부를 수 있습니다.")))
	{
		return Result;
	}

	UWorld* World = GetWorld();
	const FVector Forward = Direction.GetSafeNormal();
	if (World == nullptr || Forward.IsNearlyZero() || Range <= 0.f)
	{
		return Result;
	}

	UMassNavigationSubsystem* Navigation = World->GetSubsystem<UMassNavigationSubsystem>();
	if (Navigation == nullptr)
	{
		return Result;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	const FVector End = Start + Forward * Range;

	// 선분을 감싸는 상자로 후보를 좁힌 뒤, 개체마다 선분과의 거리로 판정한다.
	const FVector Padding(TraceRadius + MaxAgentRadiusGuess);
	TArray<FMassNavigationObstacleItem, TInlineAllocator<64>> Candidates;
	Navigation->GetObstacleGrid().Query(FBox(Start, End).ExpandBy(Padding), Candidates);

	FMassEntityHandle NearestEntity;
	FVector NearestPoint = FVector::ZeroVector;
	float NearestDistance = TNumericLimits<float>::Max();

	for (const FMassNavigationObstacleItem& Candidate : Candidates)
	{
		const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Candidate.Entity);
		const FTREnemyCombatFragment* Combat = EntityManager.GetFragmentDataPtr<FTREnemyCombatFragment>(Candidate.Entity);
		if (Transform == nullptr || Combat == nullptr)
		{
			continue;
		}

		const FVector EntityLocation = Transform->GetTransform().GetLocation();
		const FVector ClosestOnRay = FMath::ClosestPointOnSegment(EntityLocation, Start, End);

		const FAgentRadiusFragment* Radius = EntityManager.GetFragmentDataPtr<FAgentRadiusFragment>(Candidate.Entity);
		const float HitRadius = TraceRadius + (Radius ? Radius->Radius : 0.f);

		if (FVector::DistSquared(ClosestOnRay, EntityLocation) > FMath::Square(HitRadius))
		{
			continue;
		}

		const float DistanceAlongRay = static_cast<float>(FVector::Dist(Start, ClosestOnRay));
		if (DistanceAlongRay < NearestDistance)
		{
			NearestDistance = DistanceAlongRay;
			NearestEntity = Candidate.Entity;
			NearestPoint = ClosestOnRay;
		}
	}

	if (!NearestEntity.IsSet())
	{
		return Result;
	}

	FTREnemyCombatFragment& Combat = EntityManager.GetFragmentDataChecked<FTREnemyCombatFragment>(NearestEntity);
	Combat.Health -= Damage;

	Result.bHit = true;
	Result.Location = NearestPoint;
	Result.RemainingHealth = FMath::Max(0.f, Combat.Health);
	Result.bKilled = (Combat.Health <= 0.f);

	if (const FTREnemyCombatParams* Params =
		EntityManager.GetConstSharedFragmentDataPtr<FTREnemyCombatParams>(NearestEntity))
	{
		Result.EnemyType = Params->EnemyType;
	}

	if (!Result.bKilled)
	{
		return Result;
	}

	if (FTREnemyStateFragment* State = EntityManager.GetFragmentDataPtr<FTREnemyStateFragment>(NearestEntity))
	{
		EnterActionState(*State, ETREnemyActionState::Death, World->GetTimeSeconds());
	}

	OnEnemyKilled.Broadcast(NearestPoint, Combat.ResourceValue);
	EntityManager.Defer().DestroyEntity(NearestEntity);

	return Result;
}

TStatId UTRMassCombatSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTRMassCombatSubsystem, STATGROUP_Tickables);
}

void UTRMassCombatSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ConsumePendingImpacts();
}

void UTRMassCombatSubsystem::ConsumePendingImpacts()
{
	if (PendingImpacts.IsEmpty())
	{
		return;
	}

	// 피해 판정은 서버 권위다. 클라이언트에서는 큐만 비우고 아무것도 적용하지 않는다.
	// (Mass 프로세서 자체가 서버 전용이라 정상 흐름에서는 여기 들어올 일이 없지만,
	//  블루프린트에서 RequestImpact를 부를 수 있으므로 막아둔다.)
	UWorld* World = GetWorld();
	const bool bAuthoritative = World && World->GetNetMode() != NM_Client;

	// 피해 대상은 따로 등록받지 않는다. **적이 쫓던 목표가 곧 피해를 받을 대상**이라
	// 목표 관리자에게 물어보면 된다. 별도로 들고 있으면 목표를 바꿨을 때
	// 엉뚱한 액터가 계속 맞는 상태가 조용히 생긴다.
	const UTRTargetDirectorSubsystem* Director =
		World ? World->GetSubsystem<UTRTargetDirectorSubsystem>() : nullptr;

	FTRMassImpactEvent Impact;
	while (PendingImpacts.Dequeue(Impact))
	{
		if (!bAuthoritative)
		{
			continue;
		}

		AActor* DamagedActor = Director ? Director->GetTargetActor(Impact.Channel) : nullptr;

		// 기차는 착탄점으로 장갑을 찾아 흡수시키는 자체 경로를 갖고 있다.
		// 표준 TakeDamage 로 보내면 장갑을 건너뛰고 차체가 바로 맞는다.
		if (ATRTrain* Train = Cast<ATRTrain>(DamagedActor))
		{
			Train->ApplyImpactDamage(Impact.Location, Impact.Damage, Impact.Radius,
				/*EventInstigator=*/nullptr, /*DamageCauser=*/nullptr);
		}
		else if (DamagedActor != nullptr)
		{
			// 기차가 아닌 목표는 언리얼 표준 경로로 보낸다.
			FHitResult HitResult;
			HitResult.ImpactPoint = Impact.Location;
			HitResult.Location = Impact.Location;

			const FVector ShotDirection =
				(Impact.Location - DamagedActor->GetActorLocation()).GetSafeNormal();

			UGameplayStatics::ApplyPointDamage(DamagedActor, Impact.Damage, ShotDirection,
				HitResult, /*EventInstigator=*/nullptr, /*DamageCauser=*/nullptr,
				/*DamageTypeClass=*/nullptr);
		}

		// 연출은 피해 적용 여부와 무관하게 낸다. 대상을 못 찾은 상황에서도 폭발이 보여야
		// "적이 사라졌는데 아무 일도 안 일어났다"는 오진을 피한다.
		OnEnemyImpact.Broadcast(Impact);
	}
}

// TRMassCombatSubsystem.cpp

#include "Mass/Combat/TRMassCombatSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavigationSubsystem.h"
#include "Mass/Core/TRMassFragments.h"
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
	//
	// 두 점을 더해서 만든다. FBox(A, B) 생성자는 크기 비교 없이 A를 Min, B를 Max로
	// 그대로 대입하므로, 뒤나 왼쪽으로 쏘면 Min > Max인 뒤집힌 상자가 된다.
	// 그러면 그리드의 셀 순회가 한 번도 돌지 않아 **후보가 항상 0개**가 되고,
	// 맞을 적이 있어도 빗나간다. operator+= 는 축마다 알아서 갱신하므로 방향을 타지 않는다.
	FBox TraceBounds(ForceInit);
	TraceBounds += Start;
	TraceBounds += End;

	const FVector Padding(TraceRadius + MaxAgentRadiusGuess);
	TArray<FMassNavigationObstacleItem, TInlineAllocator<64>> Candidates;
	Navigation->GetObstacleGrid().Query(TraceBounds.ExpandBy(Padding), Candidates);

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

		// 이미 죽었지만 아직 안 지워진 개체는 건너뛴다.
		if (Combat->Health <= 0.f)
		{
			continue;
		}

		// 엔티티 트랜스폼은 발밑이다. 그 점 하나로 판정하면 몸통을 조준한 사격이 빗나간다 —
		// 포탑은 기차 위에 있어 광선이 몸통 높이를 지나가지 발밑을 지나지 않기 때문이다.
		// 그래서 적을 발밑에서 위로 뻗은 세로 선분으로 보고, 두 선분의 최단 거리를 잰다.
		const FVector Feet = Transform->GetTransform().GetLocation();

		const FTREnemyCombatParams* CandidateParams =
			EntityManager.GetConstSharedFragmentDataPtr<FTREnemyCombatParams>(Candidate.Entity);
		const float BodyHeight = CandidateParams ? CandidateParams->HitHeight : 0.f;

		FVector ClosestOnRay = FVector::ZeroVector;
		FVector ClosestOnBody = FVector::ZeroVector;
		FMath::SegmentDistToSegmentSafe(Start, End, Feet, Feet + FVector(0.f, 0.f, BodyHeight),
			ClosestOnRay, ClosestOnBody);

		const FAgentRadiusFragment* Radius = EntityManager.GetFragmentDataPtr<FAgentRadiusFragment>(Candidate.Entity);
		const float HitRadius = TraceRadius + (Radius ? Radius->Radius : 0.f);

		if (FVector::DistSquared(ClosestOnRay, ClosestOnBody) > FMath::Square(HitRadius))
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

	// 여기서 지우지 않는다. 사망 연출이 보이도록 시체를 남기고, 삭제는 연출 시간이 지난 뒤
	// UTRMassDeathProcessor 가 한다. 죽는 경로가 늘어나도 삭제 시점은 그 한 곳에서 정해진다.
	if (FTREnemyStateFragment* State = EntityManager.GetFragmentDataPtr<FTREnemyStateFragment>(NearestEntity))
	{
		// 진입 시각이 곧 사망 연출의 원점이 된다.
		TryEnterActionState(*State, ETREnemyActionState::Death, World->GetTimeSeconds());
	}

	// 그 자리에 멈춰야 한다. 태그 부착은 명령 버퍼를 거쳐 다음 플러시에 반영되므로,
	// 그 사이 남아 있는 속도로 미끄러지지 않도록 속도와 조종력은 여기서 직접 끊는다.
	if (FMassVelocityFragment* Velocity = EntityManager.GetFragmentDataPtr<FMassVelocityFragment>(NearestEntity))
	{
		Velocity->Value = FVector::ZeroVector;
	}
	if (FMassForceFragment* Force = EntityManager.GetFragmentDataPtr<FMassForceFragment>(NearestEntity))
	{
		Force->Value = FVector::ZeroVector;
	}

	// 이 태그가 조종·분리에서 빼주고, 삭제 프로세서가 집어갈 표시가 된다.
	EntityManager.Defer().AddTag<FTRDeadTag>(NearestEntity);

	// 자원은 처치 즉시 준다. 연출이 끝날 때까지 미루면 타격감이 늦는다.
	// 체력이 0 이하인 개체는 위에서 후보에서 걸러지므로 두 번 지급되지 않는다.
	OnEnemyKilled.Broadcast(NearestPoint, Combat.ResourceValue);

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

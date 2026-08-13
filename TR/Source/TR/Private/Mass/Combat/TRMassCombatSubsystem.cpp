// TRMassCombatSubsystem.cpp

#include "Mass/Combat/TRMassCombatSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Targeting/TRTargetDirectorSubsystem.h"

void UTRMassCombatSubsystem::Deinitialize()
{
	PendingImpacts.Empty();

	Super::Deinitialize();
}

void UTRMassCombatSubsystem::EnqueueImpact(const FTRMassImpactEvent& Impact)
{
	PendingImpacts.Enqueue(Impact);
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

		if (DamagedActor != nullptr)
		{
			// 언리얼 표준 피해 경로. 받는 쪽이 TakeDamage를 구현하지 않았으면
			// 아무 일도 일어나지 않는다(크래시하지 않는다).
			//
			// 위치를 담아 보내는 이유는 "어느 장갑이 맞았는가"를 받는 쪽이 정할 수 있게
			// 하기 위해서다. 그 판단은 기차의 구성을 아는 쪽이 해야 한다.
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

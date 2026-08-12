// TRMassCombatSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Mass/Core/TRMassTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TRMassCombatSubsystem.generated.h"

/**
 * 적이 목표에 넣는 피해 한 건.
 *
 * 자폭·상체 충돌·사격을 하나의 구조로 다룬다. 셋의 차이는 수치와 연출뿐이고
 * "언제 어디서 얼마"라는 형태는 같기 때문이다.
 */
USTRUCT(BlueprintType)
struct FTRMassImpactEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	float Damage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	ETREnemyType SourceType = ETREnemyType::Melee;

	/** 폭발 연출을 낼지. 사격 피탄은 false로 두고 다른 연출을 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	bool bExplosion = true;

	/** 이 적이 쫓던 채널. 피해를 받을 액터는 그 채널의 목표에서 찾는다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	ETRTargetChannel Channel = ETRTargetChannel::Train;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTROnEnemyImpact, const FTRMassImpactEvent&, Impact);

/**
 * Mass 엔티티가 만든 피해를 게임 스레드로 옮겨 실제 액터에 적용하는 창구.
 *
 * 프로세서는 워커 스레드에서 돌 수 있으므로 액터 함수를 직접 부르면 안 된다.
 * 그래서 프로세서는 큐에 넣기만 하고(EnqueueImpact), 소비는 여기 Tick에서 한다.
 * 이 경계가 없으면 "가끔 크래시하는" 종류의 버그가 생기고, 재현이 매우 어렵다.
 *
 * **피해를 받는 쪽은 이 시스템의 소관이 아니다.** 언리얼 표준 경로
 * (UGameplayStatics::ApplyPointDamage → AActor::TakeDamage)로 쏘기만 한다.
 * 기차가 그걸 어떻게 나눠 받을지(장갑 → 차체 등)는 기차 쪽에서 TakeDamage를
 * 구현하면 된다. 이렇게 두면 적 시스템과 기차 시스템이 서로의 헤더를 몰라도 된다.
 *
 * 연출(폭발 이펙트/사운드)은 OnEnemyImpact에 붙여서 블루프린트에서 처리한다.
 * 이펙트 스폰은 게임 스레드 작업이므로 여기 말고는 걸 자리가 없다.
 */
UCLASS(BlueprintType)
class TR_API UTRMassCombatSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 피해 한 건을 예약한다. **워커 스레드에서 불러도 안전하다** — 큐에 넣기만 한다.
	 * 실제 적용은 다음 게임 스레드 틱이다.
	 */
	void EnqueueImpact(const FTRMassImpactEvent& Impact);

	/** 블루프린트/테스트용. 게임 스레드 전용 경로도 같은 큐를 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Combat")
	void RequestImpact(const FTRMassImpactEvent& Impact) { EnqueueImpact(Impact); }

	/**
	 * 폭발/피탄 연출용. 게임 스레드에서만 발생한다.
	 *
	 * 주의 — 여기에 피해 적용을 또 붙이지 말 것. 표준 경로로 이미 나갔으므로
	 * 두 번 들어간다. 이 델리게이트는 이펙트·사운드·카메라 흔들림 몫이다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "TR|Combat")
	FTROnEnemyImpact OnEnemyImpact;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

private:
	/** 큐를 비우고 각 건을 목표 액터에 적용한다. */
	void ConsumePendingImpacts();

	/**
	 * 다중 생산자 / 단일 소비자. 프로세서 여럿이 병렬로 넣고 Tick 하나가 뺀다.
	 * 락을 쓰지 않으므로 스폰 폭주 상황에서도 프로세서가 기다리지 않는다.
	 */
	TQueue<FTRMassImpactEvent, EQueueMode::Mpsc> PendingImpacts;
};

/**
 * 프로세서 쿼리에서 이 서브시스템을 요구하기 위한 특성 선언.
 * 큐 삽입만 하므로 게임 스레드에 묶이지 않고, 동시 쓰기도 안전하다.
 */
template<>
struct TMassExternalSubsystemTraits<UTRMassCombatSubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = true
	};
};

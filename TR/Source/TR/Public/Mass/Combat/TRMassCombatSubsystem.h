// TRMassCombatSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Mass/EntityHandle.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Mass/Core/TRMassTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TRMassCombatSubsystem.generated.h"

struct FMassEntityManager;

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

	/** 폭발 반경(cm). 이 안에 든 포탑이 함께 맞는다. 사격은 착탄점만. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	float Radius = 0.f;

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

/** 포탑 한 발이 적에게 닿은 결과. */
USTRUCT(BlueprintType)
struct FTRMassHitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	bool bHit = false;

	/** 맞은 지점. 피탄 이펙트를 여기 낸다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	FVector Location = FVector::ZeroVector;

	/** 이 발로 적이 죽었는지. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	bool bKilled = false;

	/** 남은 체력. 죽었으면 0. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	float RemainingHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "TR|Combat")
	ETREnemyType EnemyType = ETREnemyType::Melee;

	/**
	 * 맞은 개체. 범위 피해에 넘겨 직격 대상을 두 번 때리지 않게 한다.
	 * 블루프린트 타입이 아니므로 UPROPERTY 가 아니다 — C++ 반환값 전용이다.
	 */
	FMassEntityHandle HitEntity;
};

/** 적을 처치했다. 자원을 받을 쪽이 여기 붙는다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTROnEnemyKilled, const FVector&, Location, int32, ResourceValue);

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

	/**
	 * 포탑 한 발. 선분에 가장 먼저 걸리는 적에게 피해를 주고 결과를 돌려준다.
	 * 게임 스레드에서만 부를 것.
	 *
	 * @param TraceRadius 탄의 굵기(cm). 적 반지름에 더해 판정한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Combat")
	FTRMassHitResult FireHitscan(const FVector& Start, const FVector& Direction, float Range,
		float Damage, float TraceRadius = 25.f);

	/**
	 * 반경 안의 적 전부에게 같은 피해를 준다. 게임 스레드에서만 부를 것.
	 * 거리 감쇠는 없다. 피해를 받은 개체 수를 돌려준다.
	 *
	 * @param IgnoreEntity 직격으로 이미 맞은 개체. FTRMassHitResult::HitEntity 를 넘긴다.
	 */
	int32 ApplyRadialDamage(const FVector& Center, float Radius, float Damage,
		FMassEntityHandle IgnoreEntity = FMassEntityHandle());

	/** 처치 통지. 자원 누적·킬 로그·이펙트가 여기 붙는다. */
	UPROPERTY(BlueprintAssignable, Category = "TR|Combat")
	FTROnEnemyKilled OnEnemyKilled;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

private:
	/** 그리드 질의 상자를 넓히는 여유(cm). 가장 큰 적의 반지름을 덮는다. */
	static constexpr float MaxAgentRadiusGuess = 200.f;

	/** 큐를 비우고 각 건을 목표 액터에 적용한다. */
	void ConsumePendingImpacts();

	/**
	 * 개체 하나에 피해를 넣고 사망까지 처리한다. 직격과 범위 피해가 이 한 곳을 함께 쓴다.
	 * 그래야 어느 경로로 죽든 자원 지급과 시체 정리가 같은 규칙을 탄다.
	 */
	FTRMassHitResult ApplyDamageToEntity(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FVector& HitPoint, float Damage);

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

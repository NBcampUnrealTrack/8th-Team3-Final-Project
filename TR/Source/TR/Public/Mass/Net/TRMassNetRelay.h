// TRMassNetRelay.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Mass/EntityHandle.h"
#include "Mass/Core/TRMassTypes.h"
#include "TRMassNetRelay.generated.h"

class UMassEntityConfigAsset;

/**
 * 적 한 마리의 복제 상태. 한 개체당 12바이트 남짓으로 눌러 담는다.
 *
 * 위치는 FVector_NetQuantize(1cm 단위)로 충분하다. 표현용이라 밀리미터가 필요 없고,
 * 피해 판정은 어차피 서버가 자기 값으로 한다.
 */
USTRUCT()
struct FTRMassAgentSnapshot
{
	GENERATED_BODY()

	/** 서버가 부여한 식별자. 클라이언트는 이 번호로 같은 적을 찾는다. */
	UPROPERTY()
	int32 NetId = 0;

	UPROPERTY()
	FVector_NetQuantize Location = FVector::ZeroVector;

	/** Yaw 를 1바이트로 압축(약 1.4도 해상도). 적의 몸통 방향은 이 정도면 충분하다. */
	UPROPERTY()
	uint8 Yaw = 0;

	/** 애니메이션을 고르는 근거. 클라이언트의 FTREnemyStateFragment 로 들어간다. */
	UPROPERTY()
	ETREnemyActionState ActionState = ETREnemyActionState::None;

	/** ConfigPalette 색인. 어떤 메시로 그릴지. */
	UPROPERTY()
	uint8 ConfigIndex = 0;
};

/**
 * Mass 적을 클라이언트에 보여주기 위한 스냅샷 중계기.
 *
 * **왜 시뮬레이션 복제가 아니라 스냅샷인가**
 *
 * 처음에는 "서버가 웨이브 시드만 내려주고 클라이언트가 같은 시뮬레이션을 돌린다"를
 * 검토했다. 대역폭이 거의 0이라 매력적이지만 두 가지에 막힌다:
 *
 *  1. **엔진이 StateTree 를 서버 전용으로 박아 두었다**
 *     (MassStateTreeTypes.h 의 ExecutionFlags = Standalone | Server). 프로세서뿐 아니라
 *     UMassStateTreeTrait 자체가 클라이언트에서 붙지 않는다. 우리 적의 행동(자폭·사출·
 *     사격)은 전부 StateTree 태스크에 있으므로, 클라이언트 개체는 목표까지 걸어간 뒤
 *     아무 일도 못 하고 영원히 쌓인다.
 *  2. 지형 스크롤·지면 트레이스가 양쪽에서 미세하게 어긋나면 위치가 계속 벌어지고,
 *     포탑 조준이 화면과 어긋난다.
 *
 * 스냅샷은 둘 다 없다. 동시 상한이 120마리라 비용도 감당된다 —
 * 120 × 12바이트 × 10Hz ≈ 14KB/s. 대신 클라이언트 개체는 **순수 표현**이고
 * 시뮬레이션을 하지 않는다.
 *
 * 클라이언트는 같은 Config 로 엔티티를 만든다. 이동·행동 프로세서는 서버 전용이라
 * 저절로 조용하고, 표현·애니메이션 프로세서(Client | Standalone)만 돈다.
 */
UCLASS()
class TR_API ATRMassNetRelay : public AInfo
{
	GENERATED_BODY()

public:
	ATRMassNetRelay();

	/** 서버 전용. 스냅샷 프로세서가 매 갱신마다 통째로 채워 넣는다. */
	void SubmitSnapshot(TArray<FTRMassAgentSnapshot>&& InAgents);

	/**
	 * Config 를 팔레트에 등록하고 색인을 돌려준다. 스폰 시 서버가 부른다.
	 * 색인은 복제되므로 클라이언트가 같은 애셋으로 표현할 수 있다.
	 */
	uint8 RegisterConfig(UMassEntityConfigAsset* Config);

	/** 스냅샷 갱신 주기(초). 서버 프로세서가 읽는다. */
	float GetSnapshotInterval() const { return SnapshotInterval; }

	/** 월드에서 릴레이를 찾는다. 서버는 없으면 만든다. */
	static ATRMassNetRelay* Get(UWorld* World, bool bCreateIfMissing);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 스냅샷 주기(초). 0.1이면 10Hz.
	 *
	 * 올리면 부드러워지지만 대역폭이 비례해서 는다. 클라이언트가 스냅샷 사이를
	 * 보간하므로 10Hz로도 끊겨 보이지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TR|Net", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float SnapshotInterval = 0.1f;

private:
	/** 클라이언트가 스냅샷을 로컬 엔티티에 반영한다. */
	void ApplySnapshotToEntities(float DeltaSeconds);

	/** 더 이상 스냅샷에 없는 로컬 엔티티를 지운다. */
	void DestroyMissingEntities(const TSet<int32>& LiveNetIds);

	UFUNCTION()
	void OnRep_Agents();

	UPROPERTY(ReplicatedUsing = OnRep_Agents)
	TArray<FTRMassAgentSnapshot> Agents;

	/** 스폰에 쓰인 Config 목록. 색인이 스냅샷의 ConfigIndex 다. */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<UMassEntityConfigAsset>> ConfigPalette;

	/** 클라이언트가 만든 로컬 엔티티. NetId → 핸들. */
	TMap<int32, FMassEntityHandle> LocalEntities;

	/** 보간용. NetId → (직전 위치, 목표 위치). 스냅샷은 띄엄띄엄 오므로 사이를 메운다. */
	struct FInterpState
	{
		FVector PreviousLocation = FVector::ZeroVector;
		FVector TargetLocation = FVector::ZeroVector;
		float PreviousYaw = 0.f;
		float TargetYaw = 0.f;
		float Alpha = 1.f;
	};
	TMap<int32, FInterpState> InterpStates;
};

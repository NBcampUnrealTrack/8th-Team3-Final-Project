// TRMassNetRelay.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Mass/EntityHandle.h"
#include "Mass/Core/TRMassTypes.h"
#include "TRMassNetRelay.generated.h"

class UMassEntityConfigAsset;
struct FMassEntityManager;
struct FTRMassNetIdFragment;
struct FTREnemyStateFragment;
struct FTREnemyVisualFragment;

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

	/**
	 * 손상도를 1바이트로 압축(0 = 멀쩡, 255 = 파괴 직전).
	 *
	 * 체력 자체가 아니라 비율만 보내는 이유는 클라이언트가 이 값으로 판정을 하지 않기
	 * 때문이다 — 머테리얼에 넣을 0~1 하나면 충분하고, 티어마다 다른 최대 체력을 함께
	 * 보낼 필요도 없어진다.
	 */
	UPROPERTY()
	uint8 DamageRatio = 0;

	// ── 굽기 / 되돌리기 ──────────────────────────────────────────────────
	//
	// **필드를 늘리면 이 두 함수를 같이 고친다.** 둘을 여기 나란히 둔 이유가 그것이다.
	//
	// 예전에는 필드 선언(이 구조체), 값 수집(UTRMassNetSnapshotProcessor), 값 적용
	// (ATRMassNetRelay) 이 서로 다른 세 파일에 흩어져 있었다. 그래서 필드를 더하고
	// 수집이나 적용 중 하나를 빠뜨려도 컴파일이 되고, 클라이언트에서 그 값만 조용히
	// 기본값으로 남았다 — 호스트에서는 재현되지 않는 종류의 버그다.
	//
	// 이제 한쪽만 고치면 바로 옆 함수가 눈에 들어온다. 컴파일러가 잡아주지는 못하지만,
	// 최소한 "다른 파일에 있는 줄 몰랐다"는 없어진다.

	/**
	 * 서버 — 엔티티의 현재 상태를 스냅샷에 굽는다.
	 *
	 * @param State   없으면 ActionState 를 None 으로 둔다(전투 구성이 아닌 개체).
	 * @param Visual  없으면 손상도를 0으로 둔다.
	 */
	void CollectFrom(const FTransform& Transform, const FTRMassNetIdFragment& NetIdFragment,
		const FTREnemyStateFragment* State, const FTREnemyVisualFragment* Visual);

	/**
	 * 클라이언트 — 스냅샷을 로컬 엔티티에 되돌린다.
	 *
	 * **위치와 회전은 여기서 다루지 않는다.** 그 둘은 스냅샷 사이를 보간해야 해서
	 * 릴레이가 들고 있는 진행도(FInterpState)가 필요하고, 그 상태는 이 구조체 밖에 있다.
	 * 여기서 처리하는 것은 보간 없이 그대로 옮기는 값들이다.
	 *
	 * @param ClientWorldTime 클라이언트의 월드 시각. **서버 시각을 넣으면 안 된다** —
	 *                        두 월드는 시간 원점이 달라서 단발 애니메이션의 경과 시간과
	 *                        피격 플래시가 엉뚱하게 계산된다.
	 */
	void ApplyTo(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float ClientWorldTime) const;
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

	/**
	 * 클라이언트 — 표현 파이프라인이 이 월드에서 실제로 도는지 확인한다.
	 *
	 * 표현/LOD 프로세서는 엔진에서 자동 등록이 꺼져 있고 기본 넷 모드도
	 * Server|Standalone 이라, 프로젝트가 DefaultMass.ini 로 켜주지 않으면
	 * **엔티티는 있는데 아무것도 그려지지 않는다.** 에러도 경고도 없다.
	 * 한 대에서 PIE 로 띄우면 두 창이 같은 설정을 쓰므로 절대 재현되지 않고,
	 * 설정이 다른 다른 컴퓨터가 붙는 순간에만 갈린다 — 그래서 여기서 확인한다.
	 */
	void VerifyClientRepresentationPipeline() const;

	/**
	 * 클라이언트 — 복제가 어디까지 왔는지 주기적으로 남긴다.
	 *
	 * 스냅샷 0 / 팔레트 0 / 엔티티 0 중 어디서 끊겼는지가 원인을 가른다.
	 * 이 로그가 없으면 "클라에서만 적이 안 보인다"는 증상 하나로
	 * 복제·애셋·표현 중 무엇이 문제인지 구분할 수가 없다.
	 */
	void LogClientDiagnostics();

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

	/** 다음 진단 로그 시각(클라이언트 월드 시간). */
	float NextDiagnosticsTime = 0.f;

	/** 이미 경고한 팔레트 색인. 같은 경고가 매 틱 쏟아지지 않게 한다. */
	TSet<uint8> WarnedConfigIndices;
};

// TRMassSkinnedCustomDataProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRMassSkinnedCustomDataProcessor.generated.h"

/**
 * 원거리 표현(인스턴스 스킨드 메시)에 손상도와 피격 플래시를 실어 보낸다.
 *
 * **왜 필요한가**
 *
 * 근거리 표현 액터는 SetCustomPrimitiveDataFloat 으로 자기 메시에 값을 직접 쓴다
 * (ATRMassEnemyActor::UpdateDamageVisuals). 그 경로는 인스턴스 메시에 없다 — 수백
 * 마리가 컴포넌트 하나를 공유하므로 "이 인스턴스만" 값을 바꾸려면 인스턴스별 커스텀
 * 데이터를 써야 하고, 그걸 채워 주는 프로세서가 엔진에는 없다.
 *
 * 이게 없으면 화면의 적 대부분(스폰 거리가 200m 라 멀리 있는 쪽이 다수다)이 맞아도
 * 아무 반응이 없고, 가까이 온 순간에만 갑자기 상처가 나타난다.
 *
 * **머테리얼 쪽 주의**
 *
 * 액터는 `Custom Primitive Data` 노드로, 인스턴스 메시는 `Per Instance Custom Data`
 * 노드로 같은 값을 읽는다. **노드가 다르다.** 한쪽만 연결하면 그 LOD 에서만 표현이
 * 나오고 전환할 때 값이 튄다. 인덱스는 양쪽 모두 아래 상수와 같아야 한다.
 *
 * **정렬 규칙(깨지기 쉬운 부분)**
 *
 * 커스텀 float 은 인스턴스별로 **덧붙이는** 방식이라, 트랜스폼을 넣는
 * UMassUpdateInstancedSkinnedMeshProcessor 와 **같은 엔티티를 같은 순서로** 훑어야
 * 짝이 맞는다. 그래서 쿼리 구성이 그 프로세서와 동일하며(손상 프래그먼트만 Optional 로
 * 추가), 값이 없는 엔티티도 0을 채워 넣어 개수를 맞춘다. 엔진 레퍼런스는
 * MetaHumanCrowd 의 UMetaHumanMassUpdateCustomDataProcessor 다.
 */
UCLASS()
class TR_API UTRMassSkinnedCustomDataProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRMassSkinnedCustomDataProcessor();

	/**
	 * 손상도가 실릴 인스턴스 커스텀 데이터 인덱스.
	 * ATRMassEnemyActor::DamageCustomDataIndex 와 같아야 두 LOD 가 같은 그림을 낸다.
	 */
	static constexpr int32 DamageCustomDataIndex = 0;

	/** 피격 플래시가 실릴 인덱스. ATRMassEnemyActor::HitFlashCustomDataIndex 와 같아야 한다. */
	static constexpr int32 HitFlashCustomDataIndex = 1;

	/** 인스턴스마다 보내는 float 개수. 위 인덱스들이 전부 들어갈 만큼이어야 한다. */
	static constexpr int32 NumCustomDataFloats = 2;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

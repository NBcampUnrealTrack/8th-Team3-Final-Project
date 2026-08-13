// TRRadiusSpawnPointsGenerator.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "Mass/Spawning/TRMassSpawnUtils.h"
#include "TRRadiusSpawnPointsGenerator.generated.h"

/**
 * MassSpawner 액터를 중심으로 스폰 위치를 흩뿌리는 Generator.
 *
 * 엔진 기본 Generator는 EQS 쿼리 애셋이나 레벨의 ZoneGraph를 요구하는데,
 * "이 지점 주변에 N마리" 같은 단순한 요구에는 과한 준비물이다.
 *
 * 모양(고리/사각/선)은 FTRSpawnAreaParams가 정의하며, 런타임 웨이브 스포너와
 * **같은 구조체·같은 계산**을 쓴다. 레벨에서 눈으로 맞춰본 배치가 웨이브에서도
 * 그대로 재현된다.
 *
 * (클래스 이름의 Radius는 고리만 지원하던 시절의 흔적이다. 레벨에 배치된
 *  MassSpawner의 인스턴스 참조가 끊기므로 이름은 바꾸지 않았다.)
 */
UCLASS(BlueprintType, meta = (DisplayName = "TR SpawnPoints Generator"))
class TR_API UTRRadiusSpawnPointsGenerator : public UMassEntitySpawnDataGeneratorBase
{
	GENERATED_BODY()

public:
	virtual void Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count,
		FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const override;

protected:
	/**
	 * 스폰 영역. 기준점과 방위는 스포너 액터의 트랜스폼에서 채워지므로 여기서 비워둔다.
	 * 액터를 회전시키면 사각·선의 방향도 함께 돈다.
	 */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	FTRSpawnAreaParams Area;

	/** 무작위 방향을 보게 한다. 끄면 전부 스포너와 같은 방향을 본다. */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	bool bRandomizeYaw = true;
};

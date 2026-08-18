// TRMassSpawnerSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Mass/Core/TRMassTypes.h"
#include "Mass/Spawning/TRMassSpawnUtils.h"
#include "Subsystems/WorldSubsystem.h"
#include "TRMassSpawnerSubsystem.generated.h"

class UMassEntityConfigAsset;

/**
 * 한 번의 스폰 요청.
 *
 * 인자를 나열하면 호출부에서 순서를 헷갈리기 쉽고 새 옵션을 넣을 때마다 시그니처가 깨진다.
 * 구조체로 묶으면 블루프린트에서도 이름을 보고 채울 수 있다.
 */
USTRUCT(BlueprintType)
struct FTRChaserSpawnParams
{
	GENERATED_BODY()

	/** 스폰할 엔티티 Config. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Spawn")
	TObjectPtr<UMassEntityConfigAsset> EnemyConfig = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Spawn", meta = (ClampMin = "0"))
	int32 Count = 10;

	/**
	 * 어디에 어떤 모양으로 흩뿌릴지. 위치 계산은 전부 이 구조체가 담당한다.
	 * 레벨 배치용 Generator도 같은 구조체를 쓰므로 두 경로의 결과가 항상 같다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Spawn")
	FTRSpawnAreaParams Area;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Spawn")
	ETRTargetChannel Channel = ETRTargetChannel::Train;

	/**
	 * 체력·피해량에 곱할 배수. 기획서의 "웨이브당 +5% 복리"가 여기로 들어온다.
	 *
	 * 티어마다 Config 애셋을 복제하지 않기 위한 장치다. 애셋을 복제하면 밸런스를
	 * 고칠 때마다 전부 손봐야 하고, 하나를 빠뜨리면 그 티어만 조용히 어긋난다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Spawn", meta = (ClampMin = "0.01"))
	float StatScale = 1.f;
};

/**
 * 런타임에 추적 엔티티를 생성/제거하는 창구.
 *
 * 레벨에 놓는 AMassSpawner는 시작 시 한 번 스폰하는 용도라, 웨이브처럼
 * "게임 중에 N마리 더" 같은 요청에는 맞지 않는다. 이 서브시스템이 그 역할을 맡는다.
 *
 * 주의: 서버에서만 호출할 것.
 */
UCLASS(BlueprintType)
class TR_API UTRMassSpawnerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** @return 실제로 생성된 수 */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	int32 SpawnChasers(const FTRChaserSpawnParams& Params);

	/** 이 서브시스템이 스폰한 엔티티 중 Count마리를 제거한다. Count가 0 이하면 전부. */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	int32 DespawnChasers(int32 Count = 0);

	/**
	 * 이 서브시스템이 스폰해서 아직 살아 있는 엔티티 수.
	 *
	 * 세는 김에 죽은 핸들을 걷어낸다. 적은 대부분 프로세서가 지우므로
	 * 정리를 여기서 하지 않으면 배열이 누적 스폰 수만큼 자란다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	int32 GetSpawnedCount();

	/**
	 * C++용 — 스폰된 핸들을 직접 받고 싶을 때.
	 * (UFUNCTION은 오버로드를 지원하지 않아 이름을 분리했다.)
	 */
	int32 SpawnChasersWithHandles(const FTRChaserSpawnParams& Params, TArray<FMassEntityHandle>& OutEntities);

protected:
	virtual void Deinitialize() override;

private:
	/** 죽은 핸들을 걷어낸다. */
	void CompactSpawnedEntities();

	/** 이 서브시스템이 만든 엔티티들. 제거 요청 때 대상이 된다. */
	TArray<FMassEntityHandle> SpawnedEntities;

	/** 스폰 호출 일련번호. 같은 프레임의 호출들이 서로 다른 난수열을 받게 한다. */
	uint32 SpawnSerial = 0;

	/**
	 * 복제 식별자 발급기. 개체 하나마다 하나씩 올라간다.
	 *
	 * 0은 "아직 없음"이라 쓰지 않으므로 전위 증가로 발급한다. 재사용하지 않는 이유는
	 * 번호가 재사용되면 클라이언트가 죽은 적의 표현을 새 적에 물려줄 수 있기 때문이다.
	 * int32 는 초당 100마리를 내도 250일 이상 버틴다.
	 */
	int32 NetIdSerial = 0;
};

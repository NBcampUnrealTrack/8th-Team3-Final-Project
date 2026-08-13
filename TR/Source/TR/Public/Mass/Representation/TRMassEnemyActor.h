// TRMassEnemyActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassActorPoolableInterface.h"
#include "Mass/Core/TRMassTypes.h"
#include "TRMassEnemyActor.generated.h"

class USkeletalMeshComponent;

/**
 * AnimBP가 읽을 값 묶음.
 *
 * 게터를 여러 개 노출하면 AnimBP가 매 프레임 여러 번 호출한다. 한 번에 복사되는
 * 구조체로 묶어 Thread Safe Update Animation에서 한 번만 읽게 한다.
 * (Event Graph에서 액터를 참조하면 애니메이션 병렬 평가가 깨진다.)
 */
USTRUCT(BlueprintType)
struct FTREnemyAnimData
{
	GENERATED_BODY()

	/**
	 * 지면 대비 속력(cm/s). 재생 속도와 로코모션 블렌드에 이 값을 쓴다.
	 * 기차가 정지하고 맵이 스크롤하므로 월드 속력을 쓰면 발이 미끄러진다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	float GroundSpeed = 0.f;

	/** 액터 기준 속도. 전후좌우 블렌드용. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	FVector LocalVelocity = FVector::ZeroVector;

	/** 이산적인 행동 상태. 로코모션은 GroundSpeed로 파생시킨다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	ETREnemyActionState ActionState = ETREnemyActionState::None;

	/** 행동 상태에 들어간 뒤 경과 시간(초). */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	float StateElapsed = 0.f;
};

/**
 * Mass 엔티티의 근거리 표현용 액터.
 *
 * 위치·회전·속도를 전부 Mass가 밀어넣는다. 이 액터는 계산을 하지 않고 값만 보관해
 * AnimBP에 노출한다. 그래서 **액터 Tick이 없다** — 개체가 수백이 되면 액터 틱만으로도
 * 프레임을 먹기 때문이다.
 *
 * Mass 표현 시스템이 LOD 전환마다 스폰/파괴하는 대신 풀에서 재사용하도록
 * IMassActorPoolableInterface를 구현한다.
 */
UCLASS()
class TR_API ATRMassEnemyActor : public AActor, public IMassActorPoolableInterface
{
	GENERATED_BODY()

public:
	ATRMassEnemyActor();

	/**
	 * Mass가 매 틱 호출한다. 액터가 위치 차분으로 속도를 역산할 필요가 없어진다.
	 * (차분 방식은 LOD 전환이나 풀 재사용으로 순간이동할 때 값이 튀는 문제도 있었다.)
	 */
	void SetMassState(const FVector& InVelocity, ETREnemyActionState InActionState,
		float InStateElapsed, float DeltaSeconds);

	/** AnimBP는 이것 하나만 읽는다. */
	UFUNCTION(BlueprintPure, Category = "TR|Enemy")
	const FTREnemyAnimData& GetAnimData() const { return AnimData; }

	/** 월드 기준 이동 속력(cm/s). 스크롤 보정이 필요 없는 로직용. */
	UFUNCTION(BlueprintPure, Category = "TR|Enemy")
	float GetWorldSpeed() const { return CurrentVelocity.Size(); }

	/** 맵 스크롤 속도. 스크롤을 구현한 쪽에서 매겨주면 발 미끄러짐이 보정된다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Enemy")
	void SetGroundVelocity(const FVector& InGroundVelocity) { GroundVelocity = InGroundVelocity; }

	// IMassActorPoolableInterface
	virtual bool CanBePooled_Implementation() override { return true; }
	virtual void PrepareForPooling_Implementation() override;
	virtual void PrepareForGame_Implementation() override;

protected:
	/**
	 * 루트. 메시를 직접 루트로 두면 상대 트랜스폼이 곧 액터 트랜스폼이 되어
	 * BP에서 메시 위치/회전을 조정할 수 없다. 캐릭터 메시는 보통 Yaw를 -90도
	 * 돌려야 정면이 맞으므로 반드시 별도 루트가 필요하다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TR|Enemy")
	TObjectPtr<USceneComponent> SceneRoot;

	/** BP에서 스켈레탈 메시와 AnimClass를 지정한다. 상대 트랜스폼도 여기서 조정. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TR|Enemy")
	TObjectPtr<USkeletalMeshComponent> MeshComp;

	/** 속도 값의 떨림을 줄인다. 0이면 보간 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "TR|Enemy", meta = (ClampMin = "0.0"))
	float SpeedInterpRate = 10.f;

private:
	/** AnimBP에 노출되는 값 묶음. Mass가 밀어넣은 값에서 파생된다. */
	FTREnemyAnimData AnimData;

	FVector CurrentVelocity = FVector::ZeroVector;

	/** 맵 스크롤 속도. 지면 대비 속력 계산에 쓰인다. */
	FVector GroundVelocity = FVector::ZeroVector;
};

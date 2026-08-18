// TRMassEnemyActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassActorPoolableInterface.h"
#include "Mass/Core/TRMassParams.h"
#include "Mass/Core/TRMassTypes.h"
#include "TRMassEnemyActor.generated.h"

class USkeletalMeshComponent;
class UNiagaraSystem;

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

	/**
	 * 단발 동작을 재생 중인가. ActionState만으로는 알 수 없다 — 상태는 전투 쪽이 계속
	 * 쥐고 있고, 표현은 Duration이 지나면 로코모션으로 되돌아간다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	bool bPlayingOneShot = false;

	/** 로코모션이 이동인가 대기인가. ISM과 같은 히스테리시스 규칙을 거친 값이다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	bool bIsMoving = false;

	/**
	 * 단발 클립의 재생 위치(초). **Sequence Evaluator의 Explicit Time에 그대로 꽂는다.**
	 * Sequence Player로 자체 누적하면 ISM과 위상이 어긋나 LOD 전환에서 포즈가 튄다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	float ActionTime = 0.f;

	/**
	 * 루프 클립의 재생 위치(초). ISM과 위상이 겹치는 값이다.
	 *
	 * 다만 로코모션은 순환 클립이라 위상이 어긋나도 잘 안 보이고, 이 값을 쓰면 재생 속도가
	 * 애셋에 고정되어 발이 미끄러진다. 기본은 GroundSpeed 기반 재생 속도 쪽을 권한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	float LocomotionTime = 0.f;
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
	 *
	 * @param Anim  ISM과 같은 ResolveAnim이 내놓은 결과. 액터는 판단하지 않고 옮기기만 한다.
	 */
	void SetMassState(const UE::TR::Enemy::FAnimContext& Context,
		const UE::TR::Enemy::FAnimResolution& Anim, float DeltaSeconds);

	/**
	 * AnimBP는 이것 하나만 읽는다.
	 *
	 * BlueprintThreadSafe가 없으면 Thread Safe Update Animation에서 부를 수 없고,
	 * 그러면 AnimBP가 Event Graph로 내려가 애니메이션 병렬 평가가 통째로 꺼진다.
	 */
	UFUNCTION(BlueprintPure, Category = "TR|Enemy", meta = (BlueprintThreadSafe))
	const FTREnemyAnimData& GetAnimData() const { return AnimData; }

	/** 월드 기준 이동 속력(cm/s). 스크롤 보정이 필요 없는 로직용. */
	UFUNCTION(BlueprintPure, Category = "TR|Enemy", meta = (BlueprintThreadSafe))
	float GetWorldSpeed() const { return CurrentVelocity.Size(); }

	/**
	 * 직전 프레임의 이동 판정. 히스테리시스를 이어가려고 Mass가 되읽는다.
	 * 액터가 자기 속력으로 다시 판정하면 ISM과 임계값이 어긋난다.
	 */
	bool WasMoving() const { return AnimData.bIsMoving; }

	/**
	 * 풀에서 갓 꺼내졌는지. 참인 프레임에 AnimBP가 상태머신을 Entry로 되돌린다 —
	 * 안 그러면 죽은 채 반납된 액터가 죽은 포즈로 되살아난다.
	 *
	 * 아무도 부르지 않아도 액터 동작에는 영향이 없다. 포즈 리셋은 AnimBP만의 관심사라
	 * 이 플래그도 여기서만 소비된다(속도 보간은 bFirstUpdate가 따로 관리한다).
	 */
	bool ConsumePoseReset()
	{
		const bool bWas = bPoseResetPending;
		bPoseResetPending = false;
		return bWas;
	}

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

	//--- VFX ---
	//Mass 사망 상태 폭팔 이펙트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TR|Enemy|VFX")
	TObjectPtr<UNiagaraSystem> DeathEffect;

	/** 속도 값의 떨림을 줄인다. 0이면 보간 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "TR|Enemy", meta = (ClampMin = "0.0"))
	float SpeedInterpRate = 10.f;

private:
	/** AnimBP에 노출되는 값 묶음. Mass가 밀어넣은 값에서 파생된다. */
	FTREnemyAnimData AnimData;

	//직전 상태. 사망 이펙트를 한 번만 재생하기 위한 추적용.
	ETREnemyActionState PreviousActionState = ETREnemyActionState::None;

	FVector CurrentVelocity = FVector::ZeroVector;

	/** 첫 SetMassState인지. 속도 보간을 건너뛰는 데만 쓰고 액터가 스스로 내린다. */
	bool bFirstUpdate = true;

	/** AnimBP가 포즈를 되돌려야 하는지. ConsumePoseReset이 내린다. */
	bool bPoseResetPending = true;
};

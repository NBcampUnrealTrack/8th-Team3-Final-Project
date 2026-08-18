// TREnemyAnimInstance.h

#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "Mass/Core/TRMassTypes.h"
#include "TREnemyAnimInstance.generated.h"

class ATRMassEnemyActor;

/**
 * 근거리 표현 액터의 AnimBP가 상속할 클래스. 값은 전부 여기서 만들고 AnimBP에는 상태머신만 남긴다.
 *
 *  - **Fast Path.** 그래프 노드의 입력이 순수 멤버 변수 읽기가 아니면 그 노드는 매 프레임
 *    블루프린트 VM을 탄다. 캐스팅은 초기화 때 한 번만 한다.
 *  - **ISM과의 일치.** 무엇을 어디서 재생할지는 Mass가 정해서 밀어넣는다. AnimBP가 다시
 *    판단하면 규칙이 갈라지고 그게 LOD 전환의 팝이 된다. 여기서는 옮기기만 한다.
 *
 * 그래프 구성:
 *  - 로코모션: Idle ↔ Run을 bIsMoving으로 전이. Run 재생 속도에 LocomotionPlayRate 연결.
 *  - 단발: Sequence **Evaluator**의 Explicit Time에 ActionTime을 꽂는다(Player 아님).
 *  - bResetPose가 참인 프레임에는 상태머신을 Entry로 되돌린다.
 */
UCLASS()
class TR_API UTREnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	//── 그래프가 읽는 값. 전부 순수 멤버 변수라 Fast Path가 유지된다 ──────────

	/** 지면 대비 속력(cm/s). 지면이 스크롤하므로 월드 속력과 다르다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	float GroundSpeed = 0.f;

	/** 액터 기준 지면 대비 속도. 전후좌우 블렌드용. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	FVector LocalVelocity = FVector::ZeroVector;

	/** 이동/대기 판정. ISM 표현과 같은 히스테리시스 규칙을 거친 결과다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	bool bIsMoving = false;

	/** Blend Poses (ETREnemyActionState)의 Active Enum Value에 꽂는다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	ETREnemyActionState ActionState = ETREnemyActionState::None;

	/** 단발 동작을 재생 중인가. 로코모션 ↔ 액션 전이 조건으로 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	bool bPlayingOneShot = false;

	/** 단발 클립의 재생 위치(초). Sequence Evaluator의 Explicit Time에 그대로 넣는다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	float ActionTime = 0.f;

	/**
	 * 루프 클립의 재생 위치(초). Evaluator에 꽂으면 LOD 경계에서 위상이 완벽히 이어진다.
	 *
	 * 대신 재생 속도가 애셋에 고정되어 개체별 속도 차이만큼 발이 미끄러진다. 둘 다는 안 된다 —
	 * 속도를 속력에 맞추는 순간 위상이 ISM과 갈라진다. 로코모션은 순환 클립이고 LOD 경계는
	 * 멀어서 위상 어긋남이 잘 안 보이므로 **기본은 Player + LocomotionPlayRate** 쪽을 권한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	float LocomotionTime = 0.f;

	/** 이동 클립의 재생 속도. 발이 흐르는 지면을 따라가게 맞춘다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	float LocomotionPlayRate = 1.f;

	/** 상태머신을 Entry로 되돌려야 하는 프레임인가. 풀 재사용 직후 한 프레임만 참이다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Enemy")
	bool bResetPose = false;

	//── 애셋마다 다른 값. AnimBP 기본값에서 지정한다 ──────────────────────────

	/**
	 * 이동 클립이 재생 속도 1일 때 내는 루트 속력(cm/s). 애님 애셋에서 재어 넣는다.
	 * 실제와 다르면 재생 속도 보정이 오히려 미끄러짐을 만든다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TR|Enemy", meta = (ClampMin = "1.0", ForceUnits = "cm/s"))
	float RunClipRootSpeed = 400.f;

	/** 재생 속도 보정의 허용 범위. 너무 벌리면 걸음이 우스워진다. */
	UPROPERTY(EditDefaultsOnly, Category = "TR|Enemy", meta = (ClampMin = "0.1"))
	float MinLocomotionPlayRate = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "TR|Enemy", meta = (ClampMin = "0.1"))
	float MaxLocomotionPlayRate = 1.6f;

private:
	/** 소유 액터. 초기화 때 한 번만 캐스팅한다. 풀에 들락거릴 뿐 파괴되지 않으므로 계속 유효하다. */
	UPROPERTY(Transient)
	TObjectPtr<ATRMassEnemyActor> OwnerEnemy;
};

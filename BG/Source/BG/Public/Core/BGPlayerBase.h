// BANG! 플레이어 캐릭터 베이스 — 컴포넌트 계층만 잡아 둔 골격이다.
//
// 계층
//   Root                USceneComponent
//     ├─ Capsule        UCapsuleComponent      충돌·이동의 기준
//     │    └─ Mesh      USkeletalMeshComponent 캡슐을 따라간다
//     ├─ PrimaryCamera  UCameraComponent
//     └─ SecondaryCamera UCameraComponent
//
// ⚠️ ACharacter 가 아니라 APawn 을 상속한다.
//    ACharacter 는 CapsuleComponent 를 RootComponent 로 **하드코딩**하므로
//    "루트 밑에 캡슐" 구조를 만들 수 없다. 대신 CharacterMovementComponent 의
//    걷기/점프/네트워크 예측도 함께 포기하는 것이므로, 이동이 필요해지면
//    UFloatingPawnMovement 를 붙이거나 ACharacter 로 갈아탈지 먼저 정할 것.
//
// 카메라를 루트에 단 이유: 캡슐 밑에 달면 캡슐의 회전·이동을 그대로 따라간다.
// 루트 기준이면 캡슐이 움직여도 시점이 고정되어, 판을 내려다보는 카드 게임 시점에 맞는다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BGPlayerBase.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UCameraComponent;

UCLASS()
class BG_API ABGPlayerBase : public APawn
{
	GENERATED_BODY()

public:
	ABGPlayerBase();

	UFUNCTION(BlueprintPure, Category = "BG|Player")
	UCapsuleComponent* GetCapsule() const { return Capsule; }

	UFUNCTION(BlueprintPure, Category = "BG|Player")
	USkeletalMeshComponent* GetMesh() const { return Mesh; }

	UFUNCTION(BlueprintPure, Category = "BG|Player")
	UCameraComponent* GetPrimaryCamera() const { return PrimaryCamera; }

	UFUNCTION(BlueprintPure, Category = "BG|Player")
	UCameraComponent* GetSecondaryCamera() const { return SecondaryCamera; }

	/**
	 * 두 카메라 중 하나만 활성 상태로 만든다.
	 *
	 * ⚠️ 둘 다 활성이면 APawn::CalcCamera 가 **먼저 찾은 것**을 쓴다. 순서는 보장되지 않으므로
	 *    "가끔 다른 시점으로 시작하는" 재현 어려운 버그가 된다. 전환은 반드시 이 함수를 거칠 것.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Player")
	void SetActiveCamera(bool bUsePrimary);

	/** 지금 PrimaryCamera 가 활성인가. */
	UFUNCTION(BlueprintPure, Category = "BG|Player")
	bool IsPrimaryCameraActive() const;

protected:
	/**
	 * 계층의 기준점. 캡슐을 루트로 쓰지 않으므로 이 컴포넌트가 액터의 원점이다.
	 * 캡슐·카메라가 모두 여기에 매달린다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Player")
	TObjectPtr<USceneComponent> Root;

	/** 충돌·이동 기준. 메시가 이 밑에 붙어 함께 움직인다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Player")
	TObjectPtr<UCapsuleComponent> Capsule;

	/** 외형. 스켈레탈 메시와 ABP 는 BP 에서 지정한다 — 베이스는 비워 둔다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Player")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** 기본 시점. 스폰 시 이쪽이 활성이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Player")
	TObjectPtr<UCameraComponent> PrimaryCamera;

	/** 보조 시점. 기본은 비활성 — 켜려면 SetActiveCamera(false). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Player")
	TObjectPtr<UCameraComponent> SecondaryCamera;
};

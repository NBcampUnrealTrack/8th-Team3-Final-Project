// BANG! 연출용 시네마틱 카메라 — 캐릭터의 B 카메라 사이를 날아다니는 로컬 액터.
//
// ── 왜 별도 액터인가 ─────────────────────────────────────────────
//  SetViewTargetWithBlend 도 뷰타깃 사이를 보간해 준다. 그것으로 부족한 이유는 셋이다.
//    1. 경로가 직선뿐이다. 원탁 반대편 좌석으로 넘어갈 때 테이블을 관통한다.
//    2. 블렌드 도중 다시 전환하면 튄다. PlayerCameraManager::DoUpdateCamera 가
//       bLockOutgoing 이 false 인 동안 나가는 뷰타깃의 POV 를 매 프레임 다시 계산하기 때문에,
//       붙잡아 둔 화면이 원래 캐릭터 쪽으로 되돌아간다.
//    3. 샷을 이어 붙일 때 카메라 매니저의 블렌드 상태와 계속 씨름해야 한다.
//
//  이 액터를 쓰면 연출 내내 **뷰타깃이 이 액터 하나로 고정**된다. 샷 사이 전환은 전부
//  액터 내부의 보간이라 카메라 매니저의 블렌드 규칙이 개입하지 않는다.
//  뷰타깃 전환은 연출 시작·종료 딱 두 번뿐이다.
//
// ── 복제하지 않는다 ──────────────────────────────────────────────
//  ⚠️ 이 액터는 클라마다 로컬로 스폰된다. 트랜스폼을 복제하지 않는다.
//     매 프레임 위치를 복제하면 대역폭 낭비인 데다 렉에서 떨린다.
//     복제되는 것은 ABGGameState 의 FBGCameraShot("의도")뿐이고, 보간은 각자 로컬에서 한다.
//     시작 시각을 서버 시간으로 맞추므로 결과는 모두 같은 순간에 같은 목표로 수렴한다.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "BGCinematicCamera.generated.h"

class ABGCharacter;

/** 샷 사이를 어떤 경로로 이동할지. */
UENUM(BlueprintType)
enum class EBGShotBlend : uint8
{
	/** 위치를 직선 보간. 두 샷이 가깝고 사이에 가릴 것이 없을 때. */
	Linear UMETA(DisplayName = "직선"),

	/**
	 * LookAtLocation 을 중심으로 감아 돈다. 원탁 반대편으로 넘어갈 때 쓴다.
	 * 직선으로 가면 테이블을 뚫고 지나간다.
	 */
	Orbit  UMETA(DisplayName = "선회")
};

/**
 * 지금 무엇을 비추고 있는가 — ABGGameState 가 복제하는 **상태**다.
 *
 * ⚠️ 이벤트가 아니라 상태인 이유는 EBGMatchEndStage 와 같다.
 *    한 번 쏘고 마는 멀티캐스트였다면, 그 순간 로딩 중이던 클라는 영영 자기 카메라에 남고
 *    스스로 복구되지 못한다. 상태면 늦게 온 클라도 OnRep 으로 현재 샷에 맞춰지고,
 *    연출이 끝나면 Target 이 null 로 복제되며 전원이 자동 복귀한다.
 */
USTRUCT(BlueprintType)
struct FBGCameraShot
{
	GENERATED_BODY()

	/** 이 캐릭터의 B 카메라를 비춘다. null 이면 연출 종료 — 각자 자기 A 카메라로 돌아간다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Camera")
	TObjectPtr<ABGCharacter> Target = nullptr;

	/**
	 * 샷이 시작된 서버 시각(GetServerWorldTimeSeconds).
	 *
	 * 진행도를 각자 0 부터 세지 않고 이 값에서 계산하는 이유: 늦게 들어온 클라도
	 * "이 샷은 이미 70% 진행됐다" 를 알 수 있어야 남들과 같은 그림을 본다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Camera")
	float StartServerTime = 0.f;

	/** 이 샷까지 날아가는 데 걸리는 시간(초). 0 이하면 즉시 도착. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Camera")
	float BlendSeconds = 1.5f;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Camera")
	EBGShotBlend Blend = EBGShotBlend::Orbit;

	bool IsActive() const { return Target != nullptr; }
};

/**
 * 연출 카메라. 클라마다 하나씩 로컬 스폰되고, 연출 중에는 이 액터가 유일한 뷰타깃이다.
 *
 * 캐릭터의 B 카메라(SecondaryCamera)는 **들여다보는 카메라가 아니라 위치·FOV 마커**로 쓴다.
 * 에디터에서 프러스텀으로 구도를 잡아 두면 이 액터가 그 값으로 날아온다.
 * 그래서 B 카메라는 끝까지 비활성으로 남는다 — 활성 카메라는 언제나 A 하나뿐이다.
 */
UCLASS()
class BG_API ABGCinematicCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	ABGCinematicCamera();

	/**
	 * 새 샷으로 이동을 시작한다.
	 *
	 * 현재 위치를 출발점으로 잡으므로, 이동 도중에 불러도 지금 화면에서 이어서 날아간다
	 * (튀지 않는다 — 이게 뷰타깃 블렌드를 쓰지 않는 이유다).
	 * ElapsedSeconds 만큼 이미 지난 것으로 치고 시작한다 — 늦게 접속한 클라를 따라잡히기 위한 것.
	 */
	void BeginShot(const FBGCameraShot& Shot, float ElapsedSeconds);

	/** 보간 없이 현재 샷 목표에 즉시 붙인다. */
	void SnapToShot(const FBGCameraShot& Shot);

	/** 선회(Orbit)의 중심이자, 필요하면 시선이 향할 지점. 보통 테이블 중앙. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Camera")
	FVector LookAtLocation = FVector::ZeroVector;

	/**
	 * 가감속 지수. 1 이면 등속, 클수록 시작·끝이 부드럽다.
	 * FMath::InterpEaseInOut 에 그대로 들어간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Camera", meta = (ClampMin = "1.0"))
	float EaseExponent = 2.f;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	/** 목표 POV 를 캐릭터의 B 카메라에서 읽는다. 실패하면 false. */
	static bool ResolveShotPov(const FBGCameraShot& Shot, FTransform& OutTransform, float& OutFov);

	/** 선회 경로 — LookAtLocation 을 축으로 방향을 slerp 하고 반지름·높이는 lerp 한다. */
	FVector OrbitLerp(const FVector& Start, const FVector& End, float Alpha) const;

	FBGCameraShot CurrentShot;

	//	출발점 — BeginShot 시점의 실제 카메라 상태다. 목표가 아니라 "지금 화면" 이라
	//	이동 중 재호출해도 이어진다.
	FTransform StartTransform = FTransform::Identity;
	float StartFov = 90.f;

	FTransform TargetTransform = FTransform::Identity;
	float TargetFov = 90.f;

	float ShotElapsed = 0.f;
	bool bShotActive = false;
};

// UBGCardWidget 의 모션 런타임 상태 — 연출이 도는 동안에만 의미가 있는 값들.
//
// 성격별로 묶어 "지금 고치는 값이 어느 연출의 것인가" 를 이름에 드러내고,
// 연출을 새로 시작할 때 되돌릴 것을 Begin() 한 곳에 모은다.
//
// ⚠️ 튜닝 값(EditAnywhere)은 여기 넣지 말 것 — WBP 에 저장된 디자이너 값이 초기화된다.
// ⚠️ USTRUCT 가 아닌 이유는 하드 UObject 참조가 없어서다. 생기면 USTRUCT 로 올리고
//    소유 쪽 멤버에 UPROPERTY 를 붙일 것(아니면 GC 가 걷어간다).

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "UI/View/BGCardFXTypes.h"

/** 덱에서 손패로 날아오는 동안의 상태. */
struct FBGCardDealRuntime
{
	/** stagger 대기(Pending)에서 남은 시간. */
	float DelayRemaining = 0.f;

	/** 등장 맥락. 대기가 끝나 나타나는 시점에 BP 로 넘어간다. */
	FBGCardDealContext Context;

	/** 자리에 도착할 때까지 true. 스냅으로 연출이 잘리는 것을 막는다. */
	bool bInProgress = false;

	FVector2D StartPosition = FVector2D::ZeroVector;
	float Elapsed = 0.f;

	/** 비행이 겨누는 지점. ⚠️ 목표를 매 프레임 그대로 읽으면 궤적이 N 자로 꺾인다. */
	FVector2D AimPosition = FVector2D::ZeroVector;

	/** 호가 휘는 방향. ⚠️ 매 프레임 다시 구하면 부호가 넘어갈 때 호가 180도 뒤집힌다. */
	FVector2D ArcDirection = FVector2D::ZeroVector;
	bool bAimInitialized = false;

	/** 숨기기 전의 가시성. ⚠️ Visible 로 하드코딩하면 WBP 설정을 덮어쓴다. */
	ESlateVisibility PreVisibility = ESlateVisibility::Visible;

	/** 새 등장을 시작한다. 앞 비행이 남긴 값은 여기서 한 번에 지운다. */
	void Begin(const FVector2D& From, float Delay, const FBGCardDealContext& InContext)
	{
		Context         = InContext;
		DelayRemaining  = Delay;
		StartPosition   = From;
		AimPosition     = From;
		Elapsed         = 0.f;
		ArcDirection    = FVector2D::ZeroVector;
		bAimInitialized = false;
		bInProgress     = true;
	}
};

/**
 * 사용한 카드가 중앙으로 올라가 머무는 동안의 상태 (Presenting + Dwelling).
 * 두 단계를 함께 두는 이유는 신호가 단계를 넘나들기 때문이다 — 날아가는 중에 도착한
 * 판정·종료 신호를 담아 뒀다가 도착하는 순간 소비한다.
 */
struct FBGCardCenterRuntime
{
	FVector2D StartPosition  = FVector2D::ZeroVector;
	FVector2D TargetPosition = FVector2D::ZeroVector;
	float StartScale = 1.f;
	float StartAngle = 0.f;
	float Elapsed    = 0.f;

	/** 중앙 대기가 끝난 뒤 날아갈 곳. */
	FVector2D PendingExitPosition = FVector2D::ZeroVector;

	float DwellRemaining = 0.f;

	/**
	 * 판정 연출 전에 카드만 보여 주는 남은 시간(GameState 의 CardRevealSeconds). ReleaseHold 때 정해진다.
	 * 0 보다 크면 판정 연출은 bPendingResolvedFX 로 밀려 있고, 대기(DwellRemaining)도 흐르지 않는다.
	 */
	float RevealRemaining = 0.f;

	/** true 면 DwellRemaining 이 줄지 않는다 — ReleaseHold 가 올 때까지 무한 대기. */
	bool bHolding = false;

	/**
	 * 중앙에 도착하면 터뜨릴 판정 연출. 날아가는 중에 ReleaseHold 가 오면 여기 담긴다.
	 * ⚠️ 도착 전에 쏘면 BP 연출이 날아가는 동안 재생되고 FinishDwell 이 버려진다.
	 */
	bool bPendingResolvedFX = false;
	FBGCardPlayContext PendingResolvedContext;

	/** 중앙에 닿기 전에 도착한 FinishDwell. 버리지 않고 도착 시 소비한다. */
	bool bDwellFinishRequested = false;

	/** 새 중앙 연출을 시작한다. 밀려 있던 앞 사용의 신호도 함께 끊는다. */
	void Begin(const FVector2D& From, float FromScale, float FromAngle,
		const FVector2D& Center, const FVector2D& ExitTo)
	{
		StartPosition       = From;
		StartScale          = FromScale;
		StartAngle          = FromAngle;
		TargetPosition      = Center;
		PendingExitPosition = ExitTo;
		Elapsed             = 0.f;
		bHolding            = true;
		ClearPendingSignals();
	}

	void ClearPendingSignals()
	{
		bPendingResolvedFX    = false;
		bDwellFinishRequested = false;
		RevealRemaining       = 0.f;
	}
};

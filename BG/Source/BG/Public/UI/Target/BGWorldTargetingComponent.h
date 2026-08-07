// BANG! 3D 대상 조준 — 좌석 보드 대신 월드의 캐릭터를 직접 클릭해 대상을 고른다.
//
// 기존 대상 선택 계약(FBGTargetRequest / OnSeatSelected)을 그대로 쓴다.
// 표현만 바뀌는 것이라 서버 경로와 카드 사용 경로는 건드리지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/View/BGSeatView.h"
#include "BGWorldTargetingComponent.generated.h"

class ABGCharacter;
class ABGPlayerController;
class UBGSeatInfoDisplayComponent;

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGWorldTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGWorldTargetingComponent();

	/**
	 * 조준을 시작한다. 이미 진행 중이면 이전 요청은 취소 통지 후 대체된다.
	 * 고를 수 있는 좌석은 Request 가 정한다 — 이 컴포넌트는 규칙을 판단하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void BeginTargeting(const FBGTargetRequest& Request);

	/** 진행 중인 조준을 접는다. OnTargetSelectionCancelled 가 발생한다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void CancelTargeting();

	UFUNCTION(BlueprintPure, Category = "BG|Target")
	bool IsTargeting() const { return bTargeting; }

	UFUNCTION(BlueprintPure, Category = "BG|Target")
	const FBGTargetRequest& GetActiveRequest() const { return ActiveRequest; }

	/** 좌클릭이 들어왔다. 조준 중이 아니거나 후보가 아니면 아무 일도 없다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Target")
	void NotifyConfirmPressed();

	/** 이 컴포넌트가 대상 선택을 맡는가. 끄면 기존 2D 좌석 보드로 돌아간다. */
	UFUNCTION(BlueprintPure, Category = "BG|Target")
	bool IsWorldTargetingEnabled() const { return bWorldTargetingEnabled; }

	UPROPERTY(BlueprintAssignable, Category = "BG|Target")
	FOnBGSeatSelected OnSeatSelected;

	UPROPERTY(BlueprintAssignable, Category = "BG|Target")
	FOnBGTargetSelectionCancelled OnTargetSelectionCancelled;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 월드 조준을 쓸지. 끄면 PlayerController 가 기존 좌석 보드로 대상을 고른다 —
	 * 조준이 막혔을 때 판을 못 돌리는 상황을 만들지 않으려고 남겨 둔 스위치다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Target")
	bool bWorldTargetingEnabled = true;

	// ── 가장자리 카메라 팬 ──

	// 커서가 화면 좌우 끝에서 이 비율 안에 들어오면 카메라가 돈다(0~0.5).
	// 위아래는 쓰지 않는다 — 손패 UI 가 화면 아래라 카드를 집을 때마다 돌아간다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|Target|Camera", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float EdgePanMargin = 0.08f;

	/** 가장자리에 완전히 닿았을 때의 회전 속도(초당 도). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Target|Camera")
	float EdgePanSpeedDeg = 90.f;

	/** 정면에서 좌우로 이만큼까지만 돈다. 0 이하면 제한 없이 돈다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Target|Camera", meta = (ClampMin = "0.0"))
	float MaxPanYawDeg = 120.f;

	/** 조준이 끝난 뒤 정면으로 돌아오는 속도(초당 도). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Target|Camera")
	float RecenterSpeedDeg = 240.f;

private:
	/** 조준 중에만 켜지는 것들을 한 번에 켜고 끈다(HUD 클릭 통과, 하이라이트, 카메라 팬). */
	void ApplyTargetingState(bool bEnter);

	/** 좌클릭을 소유 컨트롤러의 InputComponent 에 건다. 늦게 만들어지므로 재시도한다. */
	void TryBindConfirmInput();

	// HUD 가 클릭을 먹으면 월드까지 안 내려온다. 조준 중에만 통과시키고 원래 값으로 되돌린다.
	void SetHudClickThrough(bool bClickThrough);

	/** 후보 전원에 Selectable, 커서가 올라간 하나에 Aimed 를 찍는다. */
	void RefreshHighlights();

	/** 모든 후보의 하이라이트를 지운다. */
	void ClearHighlights();

	/** 커서 위치를 보고 카메라를 좌우로 돌린다. */
	void UpdateEdgePan(float DeltaTime);

	/** 조준이 끝난 뒤 카메라를 정면으로 되돌린다. */
	void UpdateRecenter(float DeltaTime);

	/** 지금 카메라에 적용된 좌우 회전을 캐릭터 카메라에 반영한다. */
	void ApplyPanYaw();

	/** 커서가 올라간 좌석이 이번 요청의 후보인가. */
	bool IsSelectableSeat(int32 SeatIndex) const;

	ABGCharacter* FindCharacterBySeat(int32 SeatIndex) const;

	ABGPlayerController* GetOwningController() const;

	UBGSeatInfoDisplayComponent* GetInfoDisplay() const;

	UPROPERTY()
	FBGTargetRequest ActiveRequest;

	//	하이라이트를 찍어 둔 캐릭터들. 조준을 끝낼 때 이 목록으로 되돌린다 —
	//	후보 좌석으로 다시 찾으면 그 사이 죽은 캐릭터의 하이라이트가 남는다.
	UPROPERTY()
	TArray<TWeakObjectPtr<ABGCharacter>> HighlightedCharacters;

	bool bTargeting = false;

	//	정면 기준 좌우 회전량. 조준이 끝나면 0 으로 돌아간다.
	float PanYaw = 0.f;

	//	팬을 얹기 전의 카메라 자세와, 그 값을 어느 카메라에서 잡았는지.
	//	폰이 바뀌면 카메라도 바뀌므로 그때 다시 잡아야 한다.
	FRotator BaseCameraRotation = FRotator::ZeroRotator;
	TWeakObjectPtr<class UCameraComponent> BaseRotationOwner;

	//	클릭을 통과시키기 전의 가시성. 되돌릴 때 이 값을 그대로 쓴다.
	UPROPERTY()
	TMap<TObjectPtr<class UWidget>, ESlateVisibility> SavedHudVisibility;

	//	좌클릭 바인딩을 이미 걸었는가. InputComponent 는 컴포넌트보다 늦게 생긴다.
	bool bConfirmInputBound = false;
};

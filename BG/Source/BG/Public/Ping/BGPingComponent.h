// 핑 발신 — 클라 요청을 서버로 넘기고, 서버 검증 후 GameState 멀티캐스트까지 책임진다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Ping/BGPingTypes.h"
#include "BGPingComponent.generated.h"

class ABGGameState;
class ABGPlayerController;
class UBGPingWheelWidget;
class UInputComponent;

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGPingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGPingComponent();

	/** 클라(소유): 핑 1건을 보낸다. 대상이 없으면 TargetSeat = INDEX_NONE. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void SendPing(FGameplayTag PingTag, int32 TargetSeat);

	/** 서버: PlayerController 의 RPC 가 넘겨준다. 검증 후 전원에게 전파한다. */
	void HandleServerSendPing(FGameplayTag PingTag, int32 TargetSeat);

	/** 이 핑의 정의. DT_BGPings 가 없으면 코드 기본값을 쓴다. */
	const FBGPingDef* FindDef(FGameplayTag PingTag) const;

	/** PlayerController 의 SetupInputComponent 가 부른다. 휠 버튼을 여기에 건다. */
	void BindInput(UInputComponent* InInputComponent);

	/** Esc·우클릭 — 진행 중인 핑 제스처(누름 대기·휠)를 모두 접는다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void CancelWheel();

	/** 공통 취소 경로. 누름 대기·휠·하이라이트·시점 잠금을 한 번에 정리한다. */
	void CancelPingGesture(const TCHAR* Reason);

	/** 아이콘 선로딩이 끝났다. 먼저 뜬 마커가 아이콘을 다시 받는다. */
	FOnBGPingIconsLoaded OnIconsLoaded;

	bool AreIconsLoaded() const { return bIconsLoaded; }

	/** 입력 없이 휠을 열어 본다(콘솔 BG.PingWheel). 실제 입력과 같은 경로를 탄다. */
	void DebugOpenWheel(int32 TargetSeat);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 비워 두면 코드 기본 정의 5종으로 동작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	TSoftObjectPtr<UDataTable> PingTable;

	/** 서버 쿨다운(초). 클라 쿨다운과 별개로 서버가 다시 막는다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping", meta = (ClampMin = "0.0"))
	float ServerCooldownSec = 0.3f;

	// ── 입력·휠 튜닝 ────────────────────────────────────────────
	/** 캐릭터 위 좌클릭으로 휠을 연다. 카드 조준 중에는 조준 확정이 우선한다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input")
	FKey PingKey = EKeys::LeftMouseButton;

	/** 비어 있으면 휠 없이 클릭(=공격)만 동작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input")
	TSubclassOf<UBGPingWheelWidget> WheelWidgetClass;

	/** 누른 뒤 이만큼(px) 움직였다 떼면 드래그로 보고 휠을 열지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float ClickDragThresholdPx = 8.f;

	/** 이 거리(px) 안에서 떼면 중앙 = 공격. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float DeadZonePx = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float ClientCooldownSec = 0.4f;

	/** 최근 RateWindowSec 동안 이 횟수까지만. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "1"))
	int32 MaxPingsPerWindow = 10;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float RateWindowSec = 10.f;

	/** 같은 대상·같은 종류를 이 시간 안에 다시 보내면 무시한다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float DuplicateIgnoreSec = 0.8f;

	/** 커서 판정 사거리(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float CursorTraceDistance = 20000.f;

	/** 보조 스윕 반경(cm). 멀리 앉은 사람도 집히게 굵게 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float CursorSweepRadius = 45.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void HandlePingPressed();

	/** 떼는 순간 — 드래그가 아니면 여기서 휠을 연다. */
	void HandlePingReleased();

	/** 카드 조준 확정·취소와 같은 클릭이면 핑을 열지 않는다. */
	UFUNCTION()
	void HandleTargetConfirmed(int32 SeatIndex, int32 ContextId);

	UFUNCTION()
	void HandleTargetCancelled(int32 ContextId);

	/** 조준 컴포넌트 델리게이트를 건다. 컴포넌트가 바뀌었으면 옛것을 풀고 다시 건다. */
	void TryBindTargeting();

	/** 제스처를 계속할 수 없는 게임 상태(판 종료·사망·턴 변경·조준 시작)인가. */
	bool ShouldAbortGesture(FString& OutReason) const;

	/** 누름 대기를 시작한 순간의 턴 좌석. 턴이 넘어가면 제스처를 접는다. */
	int32 GestureTurnSeat = INDEX_NONE;

	/** 누르고 있는 동안 원점에서 가장 멀리 간 거리(px). */
	float PressMaxDragPx = 0.f;

	bool bIconsLoaded = false;

	/** 조준 중이거나 이번 프레임에 조준이 끝났는가. */
	bool IsTargetingClick() const;

	/** 열린 휠에서 지금 가리키는 칸으로 발신하고 닫는다. */
	void ConfirmWheelSelection();

	/** 휠을 닫고 카메라·외곽선을 되돌린다. */
	void CloseWheel();

	/** 끈 방향 → 휠 칸. 데드존 안이면 Center. */
	EBGPingWheelDirection DirectionFromDrag(const FVector2D& Delta) const;

	/** 휠 칸 → 핑 태그. 정의에 없으면 무효 태그. */
	FGameplayTag TagFromDirection(EBGPingWheelDirection Direction) const;

	void OpenWheel();

	/** 휠을 잡고 있는 동안 카메라 회전을 막는다. 켠 만큼만 되돌린다. */
	void SetLookInputBlocked(bool bBlocked);

	/** 고정한 대상에 조준 외곽선을 켜고 끈다. */
	void SetTargetHighlight(int32 Seat, bool bOn);

	/** 커서 아래 좌석. 이름표 판정이 비면 굵은 스윕으로 한 번 더 찾는다. */
	int32 FindSeatUnderCursor() const;

	/** 클라 쪽 연타·중복 필터. 통과하면 보낸 것으로 기록한다. */
	bool PassesClientFilters(FGameplayTag PingTag, int32 TargetSeat);

	bool GetMouseScreenPos(FVector2D& OutPos) const;

	ABGPlayerController* GetOwningPC() const;
	ABGGameState* GetBGGameState() const;

	/** 서버: 보낼 수 있는 상황인가(좌석·생존·페이즈·태그·대상·쿨다운). */
	bool ServerValidate(FGameplayTag PingTag, int32 SenderSeat, int32 TargetSeat) const;

	/** 소프트 참조 아이콘 프리로드. 게임 중 동기 로드를 막는다. */
	void PreloadIcons();

	UPROPERTY()
	TObjectPtr<UDataTable> LoadedPingTable;

	TSharedPtr<struct FStreamableHandle> IconPreloadHandle;

	double LastServerSendTime = -1000.0;

	UPROPERTY()
	TObjectPtr<UBGPingWheelWidget> WheelWidget;

	bool bWheelOpen = false;
	bool bLookInputBlocked = false;

	//	외곽선을 켜 둔 좌석. 뗄 때 정확히 이 좌석만 끈다.
	int32 HighlightedSeat = INDEX_NONE;

	//	대상은 휠을 연 순간 고정한다 — 고르는 동안 커서가 캐릭터를 벗어나기 때문이다.
	int32 PressTargetSeat = INDEX_NONE;

	//	휠의 중심. 여기서 커서까지의 방향이 곧 선택이다.
	FVector2D PressScreenPos = FVector2D::ZeroVector;

	//	조준이 확정·취소된 프레임. 같은 좌클릭으로 휠이 열리는 것을 막는다.
	uint64 LastTargetConfirmFrame = 0;

	TWeakObjectPtr<class UBGWorldTargetingComponent> BoundTargeting;

	//	눌렀지만 아직 떼지 않았다. 떼는 순간 드래그 여부를 보고 휠을 연다.
	bool bPressPending = false;

	double LastClientSendTime = -1000.0;
	TArray<double> RecentSendTimes;
	TMap<uint64, double> LastSendByTargetAndTag;
};

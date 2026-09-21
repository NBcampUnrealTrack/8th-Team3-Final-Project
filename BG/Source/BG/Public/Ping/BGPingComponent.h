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

	/** 휠이 열려 있으면 접는다(Esc·우클릭). 열려 있지 않으면 아무것도 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Ping")
	void CancelWheel();

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
	float ServerCooldownSec = 1.0f;

	// ── 입력·휠 튜닝 ────────────────────────────────────────────
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input")
	FKey PingKey = EKeys::MiddleMouseButton;

	/** 비어 있으면 휠 없이 클릭(=공격)만 동작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input")
	TSubclassOf<UBGPingWheelWidget> WheelWidgetClass;

	/** 이 거리(px) 안에서 떼면 중앙 = 공격. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float DeadZonePx = 40.f;

	/** 휠을 띄우기 시작하는 조건 — 이만큼 끌거나 이만큼 누르고 있으면. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float WheelOpenPx = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float WheelOpenDelaySec = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float ClientCooldownSec = 1.5f;

	/** 최근 RateWindowSec 동안 이 횟수까지만. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "1"))
	int32 MaxPingsPerWindow = 4;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float RateWindowSec = 10.f;

	/** 같은 대상·같은 종류를 이 시간 안에 다시 보내면 무시한다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping|Input", meta = (ClampMin = "0.0"))
	float DuplicateIgnoreSec = 2.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void HandlePingPressed();
	void HandlePingReleased();

	/** 끈 방향 → 휠 칸. 데드존 안이면 Center. */
	EBGPingWheelDirection DirectionFromDrag(const FVector2D& Delta) const;

	/** 휠 칸 → 핑 태그. 정의에 없으면 무효 태그. */
	FGameplayTag TagFromDirection(EBGPingWheelDirection Direction) const;

	void OpenWheel();

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

	bool bPressed = false;
	bool bWheelOpen = false;

	//	대상은 Press 시점에 고정한다 — 끄는 동안 커서가 캐릭터를 벗어나기 때문이다.
	int32 PressTargetSeat = INDEX_NONE;
	FVector2D PressScreenPos = FVector2D::ZeroVector;
	double PressTime = 0.0;

	double LastClientSendTime = -1000.0;
	TArray<double> RecentSendTimes;
	TMap<uint64, double> LastSendByTargetAndTag;
};

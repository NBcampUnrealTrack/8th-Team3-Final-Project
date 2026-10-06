// 핑 표시 — GameState 의 핑 통지를 받아 채팅 로그와 머리 위 핀 마커로 옮긴다(보는 쪽 전용).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Ping/BGPingTypes.h"
#include "BGPingDisplayComponent.generated.h"

class ABGCharacter;
class ABGGameState;
class ABGPlayerController;
class UBGPingMarkerWidget;
class UWidgetComponent;

/** 화면에 떠 있는 마커 1개. 같은 좌석에 여러 개가 말풍선처럼 쌓인다. */
USTRUCT()
struct FBGPingMarker
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UWidgetComponent> Widget;

	//	마커가 붙은 좌석. 재배치는 이 좌석 단위로 한다.
	int32 AnchorSeat = INDEX_NONE;

	//	마커를 세울 때의 좌석 폰. 좌석 폰이 바뀌면 옛 폰의 마커는 버린다.
	TWeakObjectPtr<AActor> AnchorActor;

	//	선로딩 전에 뜬 마커의 아이콘을 나중에 채우려고 들고 있다.
	FGameplayTag PingTag;

	//	낮을수록 새 것. 0 이 가운데고 1·2 가 좌우로 벌어진다.
	int32 Slot = 0;

	//	같은 좌석 마커가 짝수 개면 반 칸 옮겨 스택 전체를 가운데 맞춘다.
	float StackShift = 0.f;

	//	만료 시각(월드 시간). 타이머 대신 틱이 처리한다.
	double ExpireAt = 0.0;

	//	마지막으로 적용한 화면 변환 — 바뀔 때만 다시 넣는다.
	FVector2D AppliedOffset = FVector2D(TNumericLimits<float>::Max());
	float AppliedScale = -1.f;
	float AppliedAngle = 0.f;
	float AppliedOpacity = -1.f;
	float AppliedZ = -1.f;
};

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGPingDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGPingDisplayComponent();

	/** 좌석의 표시 색. 좌석 고정 팔레트(채팅 점·배지와 같은 표). */
	UFUNCTION(BlueprintPure, Category = "BG|Ping")
	FLinearColor GetSeatColor(int32 Seat) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 비어 있으면 마커 없이 로그만 남는다. BP 컨트롤러 디폴트에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	TSubclassOf<UBGPingMarkerWidget> MarkerWidgetClass;

	/** 위쪽에 배지 잘림 여유를 둔 크기. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	FVector2D MarkerDrawSize = FVector2D(176.f, 176.f);

	/** 마커를 거는 월드 높이(캐릭터 루트 기준 cm). 0 이하면 이름표와 같은 지점(캡슐 꼭대기 + 40). 콘솔 BG.Ping.MarkerZ 가 우선한다. */
	UPROPERTY(EditAnywhere, Category = "BG|Ping|Layout")
	float MarkerHeightZ = 0.f;

	/** 거는 지점에서 화면 위로 올리는 양(슬레이트 단위). 거리와 무관하게 이름표 위 여백이 같다. 콘솔 BG.Ping.Lift 가 우선한다. */
	UPROPERTY(EditAnywhere, Category = "BG|Ping|Layout")
	float MarkerLift = 24.f;

	/** 스택 중심을 이름표 중심에 맞추는 화면 가로 보정(슬레이트 단위). 콘솔 BG.Ping.OffsetX 가 우선한다. */
	UPROPERTY(EditAnywhere, Category = "BG|Ping|Layout")
	float MarkerOffsetX = -110.f;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping", meta = (ClampMin = "1"))
	int32 MaxMarkers = 6;

	/** 한 좌석에 동시에 쌓이는 마커 수. 넘치면 가장 오래된 것부터 지운다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping", meta = (ClampMin = "1"))
	int32 MaxMarkersPerSeat = 3;

	/** 옆으로 벌리는 화면 간격(슬레이트 단위). 거리와 무관하다. 콘솔 BG.Ping.Spacing 이 우선한다. */
	UPROPERTY(EditAnywhere, Category = "BG|Ping|Layout", meta = (ClampMin = "0.0"))
	float MarkerSpacing = 132.f;

	/** 슬롯마다 더해지는 기울기(도). 콘솔 BG.Ping.Tilt 가 우선한다. */
	UPROPERTY(EditAnywhere, Category = "BG|Ping|Layout")
	float MarkerTiltDeg = 0.f;

	/** 마커 화면 배율. 콘솔 BG.Ping.Scale 이 우선한다. */
	UPROPERTY(EditAnywhere, Category = "BG|Ping|Layout", meta = (ClampMin = "0.1"))
	float MarkerScale = 1.3f;

	/** 만료 직전 사라지는 시간(초). */
	UPROPERTY(EditAnywhere, Category = "BG|Ping|Layout", meta = (ClampMin = "0.0"))
	float FadeOutSec = 0.25f;

private:
	/** 틱당 한 번 읽는 배치 값(콘솔 → 컴포넌트 순). */
	struct FLayout
	{
		float Spacing = 0.f;
		float Lift = 0.f;
		float OffsetX = 0.f;
		float Tilt = 0.f;
		float Scale = 1.f;
	};
	FLayout ReadLayout() const;

	/** GameState 는 클라에 늦게 도착한다. 잡을 때까지 주기적으로 다시 시도한다. */
	void TryBindGameState();

	void HandlePingReceived(const FBGPingEvent& Event);

	/** 선로딩이 끝나면 아이콘 없이 떠 있던 마커를 채운다. */
	void HandleIconsLoaded();

	
	/** Accepted ping notification sound, played once on each local client. */
	void PlayPingSound(const FBGPingEvent& Event) const;

	/** 채팅 로그 한 줄. "P7 이름 → P5 이름 : 보호" */
	void AddChatLine(const FBGPingEvent& Event) const;

	/** 서버가 실어 보낸 이름으로 짧은 표시 이름을 만든다. "P3 이름앞8자…" — 이름이 없으면 "P3". */
	static FString MakeSeatLabel(int32 Seat, const FString& Name);

	/** 대상(없으면 발신자) 캐릭터 위에 마커를 세운다. */
	void ShowMarker(const FBGPingEvent& Event);

	void RemoveMarkerAt(int32 Index);

	/** 무효·만료·폰 교체된 마커를 정리하고 좌석마다 슬롯을 다시 매긴다. */
	void RepackMarkers();

	/** 이 마커를 지금 지워야 하는가(위젯 무효·만료·좌석 폰 교체). */
	bool IsMarkerStale(const FBGPingMarker& Marker, double Now) const;

	/** 슬롯 하나의 화면 오프셋·기울기·페이드를 적용한다. 값이 바뀔 때만 넣는다. */
	void ApplyMarkerTransform(FBGPingMarker& Marker, double Now, const FLayout& Layout) const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	ABGCharacter* GetSeatCharacter(int32 Seat) const;

	/** 마커를 거는 월드 높이. 콘솔 값 → 컴포넌트 값 → 캡슐 꼭대기 + 40 순. */
	float GetTunedHeightZ(const AActor* Character) const;

	//	새 것이 앞에 온다 — 화면 배치와 정리 순서가 이 순서를 따른다.
	UPROPERTY()
	TArray<FBGPingMarker> ActiveMarkers;

	UPROPERTY()
	TWeakObjectPtr<ABGGameState> BoundGameState;

	bool bIconsBound = false;

	FTimerHandle BindRetryTimer;
};

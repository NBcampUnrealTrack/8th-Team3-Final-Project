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

/** 화면에 떠 있는 마커 1개. 키는 발신 좌석 — 같은 사람의 새 핑은 이걸 교체한다. */
USTRUCT()
struct FBGPingMarker
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UWidgetComponent> Widget;

	FTimerHandle ExpireTimer;
};

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGPingDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGPingDisplayComponent();

	/** 좌석의 표시 색. 직업 DT 의 PlayerColor, 미지정이면 좌석 팔레트. */
	UFUNCTION(BlueprintPure, Category = "BG|Ping")
	FLinearColor GetSeatColor(int32 Seat) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 비어 있으면 마커 없이 로그만 남는다. BP 컨트롤러 디폴트에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	TSubclassOf<UBGPingMarkerWidget> MarkerWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	FVector2D MarkerDrawSize = FVector2D(96.f, 128.f);

	/** 캐릭터 루트 기준 마커 높이. 이름표 위로 올린다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping")
	float MarkerHeightZ = 210.f;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Ping", meta = (ClampMin = "1"))
	int32 MaxMarkers = 6;

private:
	/** GameState 는 클라에 늦게 도착한다. 잡을 때까지 주기적으로 다시 시도한다. */
	void TryBindGameState();

	void HandlePingReceived(const FBGPingEvent& Event);

	/** 채팅 로그 한 줄. "3번 → 5번: 공격" */
	void AddChatLine(const FBGPingEvent& Event) const;

	/** 대상(없으면 발신자) 캐릭터 위에 마커를 세운다. */
	void ShowMarker(const FBGPingEvent& Event);

	void RemoveMarker(int32 SenderSeat);

	ABGCharacter* GetSeatCharacter(int32 Seat) const;

	UPROPERTY()
	TMap<int32, FBGPingMarker> MarkersBySender;

	UPROPERTY()
	TWeakObjectPtr<ABGGameState> BoundGameState;

	FTimerHandle BindRetryTimer;
};

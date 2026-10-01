// 핑 5종의 공용 타입 — 정의 행(FBGPingDef), 전파 이벤트(FBGPingEvent), 휠 방향.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "BGPingTypes.generated.h"

class UTexture2D;

BG_API DECLARE_LOG_CATEGORY_EXTERN(LogBGPing, Log, All);

/** 휠에서 이 핑이 놓이는 자리. Center 는 짧은 클릭(데드존)과 같다. */
UENUM(BlueprintType)
enum class EBGPingWheelDirection : uint8
{
	Center UMETA(DisplayName = "중앙"),
	Up     UMETA(DisplayName = "위"),
	Right  UMETA(DisplayName = "오른쪽"),
	Down   UMETA(DisplayName = "아래"),
	Left   UMETA(DisplayName = "왼쪽")
};

/** 핑 1종의 정의. DT_BGPings 의 Row 이자 코드 기본값의 단위다. */
USTRUCT(BlueprintType)
struct FBGPingDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Ping"))
	FGameplayTag PingTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Label;

	/** 좌석 대상이 반드시 필요한가. Attack·Suspect·Protect 만 true. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bNeedsTarget = false;

	/** 휠·마커 가운데 아이콘. 소프트 참조 — 컴포넌트가 미리 로드한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	/** 핑 종류 색. 마커 테두리(보낸 사람 색)와는 별개다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor IconColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Audio"))
	FGameplayTag AudioTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float LifetimeSec = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EBGPingWheelDirection WheelDirection = EBGPingWheelDirection::Center;
};

/** 서버가 전원에게 보내는 핑 1건. */
USTRUCT(BlueprintType)
struct FBGPingEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SenderSeat = INDEX_NONE;

	/** 대상이 없는 핑(Warning·Heal)은 INDEX_NONE. 표시는 발신자 위에 붙는다. */
	UPROPERTY(BlueprintReadOnly)
	int32 TargetSeat = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag PingTag;

	UPROPERTY(BlueprintReadOnly)
	float ServerTime = 0.f;

	/** 서버가 채운 발신자·대상 표시 이름. 모든 머신이 같은 기록을 갖게 수신 쪽 조회를 쓰지 않는다. */
	UPROPERTY(BlueprintReadOnly)
	FString SenderName;

	UPROPERTY(BlueprintReadOnly)
	FString TargetName;
};

DECLARE_MULTICAST_DELEGATE(FOnBGPingIconsLoaded);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBGPingReceived, const FBGPingEvent& /*Event*/);

namespace BGPing
{
	/** DT 가 없을 때 쓰는 코드 기본 정의 5종. DT_BGPings 가 있으면 그쪽이 우선이다. */
	BG_API const TArray<FBGPingDef>& GetDefaultDefs();

	BG_API const FBGPingDef* FindDefaultDef(FGameplayTag PingTag);

	/** 좌석 고정 팔레트 색. 마커 배지·채팅 점이 모두 이 표를 쓴다. */
	BG_API FLinearColor GetSeatColor(int32 Seat);

	/** "Attack" 같은 짧은 이름이나 전체 태그 문자열을 Ping.* 태그로. 없으면 무효 태그. */
	BG_API FGameplayTag ResolvePingTag(const FString& Name);
}

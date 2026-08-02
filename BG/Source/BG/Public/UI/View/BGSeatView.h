// BANG! 좌석(다른 플레이어) UI 전용 뷰모델 + 대상 선택 계약.
//
// FBGCardView 와 같은 원칙: UI 계층은 ABGPlayerState 도 ASC 도 DT 도 모른다.
// 게임 로직이 UBGSeatViewLibrary 로 변환해 밀어넣고, UI 는 좌석 번호만 돌려준다.

#pragma once

#include "CoreMinimal.h"
#include "BGSeatView.generated.h"

/**
 * 좌석 1개(플레이어 1명)의 공개 정보.
 *
 * ⚠️ 여기 들어가는 값은 전부 "전원에게 공개되어도 되는 것"이어야 한다.
 *    손패 내용(FBGCardView)은 절대 넣지 말 것 — HandCount 만 넣는다.
 */
USTRUCT(BlueprintType)
struct FBGSeatView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	int32 SeatIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	FText PlayerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	int32 Health = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	int32 MaxHealth = 0;

	/** 남의 손패는 장수만 안다. 내용은 COND_OwnerOnly 라 복제되지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	int32 HandCount = 0;

	/** 장착 중인 파란 카드 이름들. 파란 카드는 공개 정보다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	TArray<FText> EquipmentNames;

	/** 공개된 역할만 채운다(보안관, 또는 사망자). 비공개면 빈 텍스트. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	FText RoleName;

	/**
	 * 보는 사람 기준 거리. 자기 자신이거나 계산 불가면 INDEX_NONE (화면에는 "-").
	 * 살아 있는 좌석만으로 이루어진 원의 최단 걸음 수다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	int32 Distance = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	bool bIsAlive = true;

	/** 이 좌석이 화면을 보고 있는 본인인가. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	bool bIsLocalPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	bool bIsCurrentTurn = false;

	bool IsValid() const { return SeatIndex != INDEX_NONE; }
};

/**
 * 대상 좌석 선택 요청.
 *
 * 어느 좌석을 고를 수 있는지는 UI 가 판단하지 않는다 — 사거리·생존·자기 자신 제외 같은
 * 규칙은 전부 게임 로직이 계산해 SelectableSeats 로 넘긴다. UI 는 그 목록만 클릭 가능하게 한다.
 */
USTRUCT(BlueprintType)
struct FBGTargetRequest
{
	GENERATED_BODY()

	/** 호출자가 응답을 짝지을 때 쓰는 값. 보통 카드 InstanceId 를 넣는다. 그대로 돌아온다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Target")
	int32 ContextId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Target")
	FText Prompt;

	/** 클릭 가능한 좌석 번호. 비어 있으면 아무것도 고를 수 없다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Target")
	TArray<int32> SelectableSeats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Target")
	bool bAllowCancel = true;

	bool IsValid() const { return ContextId != INDEX_NONE; }
};

/** 대상 좌석이 확정됐다. SeatIndex = 고른 좌석, ContextId = 요청 때 넘긴 값. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBGSeatSelected, int32, SeatIndex, int32, ContextId);

/** 대상 선택이 취소됐다(취소 버튼/우클릭 등). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGTargetSelectionCancelled, int32, ContextId);

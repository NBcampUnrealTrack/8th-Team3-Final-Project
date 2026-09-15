// BANG! 좌석(다른 플레이어) UI 전용 뷰모델 + 대상 선택 계약.
//
// FBGCardView 와 같은 원칙: UI 계층은 ABGPlayerState 도 ASC 도 DT 도 모른다.
// 게임 로직이 UBGSeatViewLibrary 로 변환해 밀어넣고, UI 는 좌석 번호만 돌려준다.

#pragma once

#include "CoreMinimal.h"
#include "Card/BGCardPresentationTypes.h"
#include "GameplayTagContainer.h"
#include "BGSeatView.generated.h"

/** 장착 중인 파란 카드 한 장. 이름도 설명도 공개 정보다. */
USTRUCT(BlueprintType)
struct FBGEquipmentView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	FText Name;

	/** DT_BGCard 의 Description. 비어 있을 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	FText Description;
};

/**
 * 좌석 1개(플레이어 1명)의 공개 정보.
 *
 * 주의: 여기 들어가는 값은 전부 "전원에게 공개되어도 되는 것"이어야 한다.
 * 손패 내용(FBGCardView)은 절대 넣지 말 것 — HandCount 만 넣는다.
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

	/** 장착 중인 파란 카드. 파란 카드는 공개 정보다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	TArray<FBGEquipmentView> Equipment;

	/** 공개된 역할만 채운다(보안관, 또는 사망자). 비공개면 빈 텍스트. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	FText RoleName;

	/** RoleName 과 같은 조건으로 채운다. 툴팁이 설명을 찾는 키다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	FGameplayTag RoleTag;

	/** 직업 이름(Character.Job.*). 역할과 달리 공개 정보라 항상 채운다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	FText JobName;

	/** JobName 과 같은 값의 태그. 아이콘처럼 이름이 아니라 종류로 고르는 곳이 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	FGameplayTag JobTag;

	/** DT_BGCharacterRow 의 Description. 직업이 무엇을 하는지 한 줄 설명이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	FText JobDescription;

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

	// 판이 돌고 있는가. 로비에도 어트리뷰트는 이미 있어서 체력만 보고는 구분되지 않는다 —
	// 스탯을 언제부터 보여줄지는 이 값으로 정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	bool bMatchRunning = false;

	// 접속이 끊긴 채 자리만 남아 있다. 죽은 것이 아니라 복귀를 기다리는 중이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seat")
	bool bIsDisconnected = false;

	bool IsValid() const { return SeatIndex != INDEX_NONE; }

	/** 이름만 뽑는다. 목록을 한 줄로 잇는 곳이 여럿이라 여기 둔다. */
	TArray<FText> GetEquipmentNames() const
	{
		TArray<FText> Names;
		Names.Reserve(Equipment.Num());
		for (const FBGEquipmentView& Item : Equipment)
		{
			Names.Add(Item.Name);
		}
		return Names;
	}
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

	/** 월드 대상선의 시각 계열. 2D 좌석 보드는 이 값을 무시한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Target")
	EBGTargetSelectionStyle PresentationStyle = EBGTargetSelectionStyle::Basic;

	bool IsValid() const { return ContextId != INDEX_NONE; }
};

/** 대상 좌석이 확정됐다. SeatIndex = 고른 좌석, ContextId = 요청 때 넘긴 값. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBGSeatSelected, int32, SeatIndex, int32, ContextId);

/** 대상 선택이 취소됐다(취소 버튼/우클릭 등). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGTargetSelectionCancelled, int32, ContextId);

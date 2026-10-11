#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BGChatEntry.generated.h"

USTRUCT(BlueprintType)
struct FBGChatEntry
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "BG|Chat")
	FString SenderName;
	
	UPROPERTY(BlueprintReadOnly, Category = "BG|Chat")
	FString Message;
	
	UPROPERTY(BlueprintReadOnly, Category = "BG|Chat")
	FDateTime Timestamp;
	
	UPROPERTY(BlueprintReadOnly, Category = "BG|Chat")
	bool bSystem = false;

	/** 관전자(사망자)끼리의 채팅이다. 연한 색으로 그린다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Chat")
	bool bSpectator = false;

	/** 핑으로 생긴 줄이다. 아이콘·색을 붙일 때 이 값을 본다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Chat")
	bool bPing = false;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Chat")
	FGameplayTag PingTag;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Chat")
	int32 SenderSeat = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Chat")
	int32 TargetSeat = INDEX_NONE;

	/** 핑 줄 앞 색 점. 마커 띠와 같은 보낸 사람 색이다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Chat")
	FLinearColor SenderColor = FLinearColor::White;
};

UENUM(BlueprintType)
enum class EBGChatFilter : uint8
{
	All UMETA(DisplayName = "전체"),
	Player UMETA(DisplayName = "대화"),
	System UMETA(DisplayName = "사용 내역"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGChatEntryReceived, const FBGChatEntry& , Entry);

#pragma once

#include "CoreMinimal.h"
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
};

UENUM(BlueprintType)
enum class EBGChatFilter : uint8
{
	All UMETA(DisplayName = "전체"),
	Player UMETA(DisplayName = "대화"),
	System UMETA(DisplayName = "사용 내역"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGChatEntryReceived, const FBGChatEntry& , Entry);

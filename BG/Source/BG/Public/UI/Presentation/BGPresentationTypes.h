// 연출 큐가 주고받는 타입.
//
// ⚠️ Kind 는 닫힌 열거형으로 둔다. 무타입 이벤트 버스(FName 키)로 열어 두면
//    빠진 처리를 컴파일러가 못 잡고, 몇 달 뒤 누가 무엇을 듣는지 추적이 안 된다.

#pragma once

#include "CoreMinimal.h"
#include "UI/View/BGCardFXTypes.h"
#include "BGPresentationTypes.generated.h"

/** 연출의 종류. 1단계는 카드 사용만 — 드로우·사망은 다음 단계에서 붙는다. */
UENUM(BlueprintType)
enum class EBGPresentationKind : uint8
{
	None     UMETA(DisplayName = "없음"),
	CardPlay UMETA(DisplayName = "카드 사용"),
};

/**
 * 연출이 끝나기를 기다리는 표. 구독자가 Hold 로 받아 두었다가 Release 로 놓는다.
 * 전부 놓여야 디렉터가 다음 연출로 넘어간다.
 */
USTRUCT(BlueprintType)
struct FBGPresentationHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BG|Presentation")
	int32 Id = INDEX_NONE;

	bool IsValid() const { return Id != INDEX_NONE; }
};

/** 연출 하나의 요청. */
USTRUCT(BlueprintType)
struct FBGPresentationRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BG|Presentation")
	EBGPresentationKind Kind = EBGPresentationKind::None;

	/** 카드 사용 연출의 맥락. 등급·카테고리·효과 목록이 여기 실린다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Presentation")
	FBGCardPlayContext Context;

	/**
	 * 이 연출의 상한(초).
	 * ⚠️ 구독자가 Release 를 빠뜨리면 큐가 영영 안 흐르는데 **에러가 나지 않는다.**
	 *    상한이 그것을 드러내고 판을 계속 굴린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Presentation", meta = (ClampMin = "0.1"))
	float TimeoutSeconds = 8.f;
};

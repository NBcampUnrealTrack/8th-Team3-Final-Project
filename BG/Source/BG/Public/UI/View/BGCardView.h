// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BGCardView.generated.h"

/**
 * UI 전용 카드 뷰모델.
 *
 * 게임 로직 쪽 카드 타입(FBGCardInstance / FBGCardRow)에 의존하지 않는다.
 * 변환은 UBGCardViewLibrary::MakeCardView 한 곳에서만 일어난다 — 그 함수가 로직/UI 경계다.
 */
USTRUCT(BlueprintType)
struct FBGCardView
{
	GENERATED_BODY()

	/** 게임 로직의 카드 인스턴스 ID. 서버에 "이 카드를 사용" 을 모호함 없이 지정하기 위한 키. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	int32 InstanceId = INDEX_NONE;

	/** 카드 종류 키 (데이터 테이블 Row 이름 등). 로직/디버깅용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	FName CardId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	FText Description;

	/**
	 * 대상 좌석을 지정해야 하는 카드인가 (BANG!, 당황, 캣 발루, 결투 등).
	 * UI는 이 값을 그대로 전달만 한다. 대상 지정 플로우는 이 작업 범위가 아니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	bool bNeedsTarget = false;

	/** 클라이언트 측 표시용 힌트일 뿐이다. 실제 사용 가능 판정은 항상 서버 권위. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card")
	bool bPlayable = true;

	bool IsValid() const { return InstanceId != INDEX_NONE; }
};

/**
 * 카드 사용 요청이 발생했을 때 브로드캐스트된다.
 *
 * 권장 바인딩 지점은 UBGHandWidget::OnCardPlayRequested 한 곳이다.
 * 핸드와 플레이 존 양쪽에 모두 바인딩하면 같은 요청을 두 번 받게 된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGCardPlayRequested, const FBGCardView&, Card);

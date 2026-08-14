// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "BGCardType.generated.h"

/** 덱에 실재하는 카드 1장. DT Row 가 아니라 런타임 인스턴스. */
USTRUCT(BlueprintType)
struct FBGCardInstance
{
	GENERATED_BODY()

	// 덱 생성 시 1부터 부여. UI 의 FBGCardView::InstanceId 와 같은 키다.
	UPROPERTY(BlueprintReadOnly) 
	int32 InstanceId = INDEX_NONE;
	
	// DT_BGCard Row 이름
	UPROPERTY(BlueprintReadOnly)
	FName CardId;

	/**
	 * 1회성 카드인가 (암살자가 대성공으로 얻는 카드).
	 *
	 * 덱에서 나온 카드가 아니라 규칙이 만들어 준 카드다. 두 가지가 다르다:
	 *   · 사용해도 **카드 사용 한도를 소모하지 않는다**
	 *   · 사용하면 버린 패로 가지 않고 **사라진다** (덱 총량이 늘지 않는다)
	 *
	 * ⚠️ InstanceId 가 **음수**다. 덱에서 나온 카드는 1부터 올라가므로 겹치지 않는다.
	 *    InstanceId 로 카드를 찾는 코드가 부호를 가정하지 않도록 주의할 것.
	 */
	UPROPERTY(BlueprintReadOnly)
	bool bEphemeral = false;

	bool IsValid() const { return InstanceId != INDEX_NONE; }
};

/**
 * "다음 1회" 대기 상태 하나 (Status.* 태그 + 수치).
 *
 * 태그를 식별자로 쓰되 저장은 여기 — 복제되고 수치를 함께 담을 수 있다.
 * 발동되면 제거된다(소모성). 같은 태그의 중첩은 허용하지 않는다.
 */
USTRUCT(BlueprintType)
struct FBGPendingStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag Tag;

	//	의미는 태그마다 다르다. 약점 포착이면 피해 증가량, 강제 판정이면 쓰이지 않는다.
	UPROPERTY(BlueprintReadOnly)
	int32 Amount = 0;
};

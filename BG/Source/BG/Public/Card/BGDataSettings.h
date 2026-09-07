// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "BGDataSettings.generated.h"

class UDataTable;
/**
 * 
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "BG Data"))
class BG_API UBGDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UBGDataSettings();


	UPROPERTY(EditAnywhere, Config, Category = "Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGCharacterRow"))
	TSoftObjectPtr<UDataTable> CharacterTable;

	UPROPERTY(EditAnywhere, Config, Category = "Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGCardRow"))
	TSoftObjectPtr<UDataTable> CardTable;

	/**
	 * 버프/디버프 이펙트 표 (FBGStatusFXRow). 장비 오라와 상태 표식이 여기서 나온다.
	 *
	 * 비어 있어도 된다 — 이펙트가 하나도 안 뜰 뿐 규칙은 그대로 돌아간다.
	 * 그래서 CardTable 과 달리 로드 실패를 check() 로 막지 않는다.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGStatusFXRow"))
	TSoftObjectPtr<UDataTable> StatusFXTable;

	/** 카드 기능과 분리된 시네마틱 Niagara 매핑 표 (FBGCardFXRow). */
	UPROPERTY(EditAnywhere, Config, Category = "Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGCardFXRow"))
	TSoftObjectPtr<UDataTable> CardFXTable;

	/**
	 * 암살자가 대성공 시 받는 카드 (Card.Id.*). 기본값은 그림자 일격.
	 *
	 * 코드에 카드 이름을 박지 않으려고 설정으로 뺐다. 이 Row 가 DT 에 없으면 카드가 지급되지
	 * 않고 경고만 남는다 — 규칙은 그대로 돌아간다.
	 *
	 * ⚠️ **일회용 카드를 가리켜야 한다**(`Card.Trait.Ephemeral`). 일반 카드를 넣으면
	 *    지급은 되지만 소비 시 버린 패로 가서 **덱 총량이 판마다 늘어난다.**
	 *    `GrantCards` 가 그때 경고를 남긴다.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Data", meta = (Categories = "Card.Id"))
	FGameplayTag EphemeralAttackCardId;
};

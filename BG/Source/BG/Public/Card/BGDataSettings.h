// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
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
};

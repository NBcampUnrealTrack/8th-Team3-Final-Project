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
};

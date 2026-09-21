// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "BGDataSettings.generated.h"

class UDataTable;
class UBGPresentationDataAsset;
class USoundClass;
class USoundMix;
class UStaticMesh;
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

	/** 카드와 무관한 공통 연출 카탈로그. 아트 작업자는 이 DataAsset 안의 참조를 교체한다. */
	UPROPERTY(EditAnywhere, Config, Category = "Presentation")
	TSoftObjectPtr<UBGPresentationDataAsset> PresentationData;

	/** UI·시스템·BGM 태그를 USoundBase 에 연결하는 표 (FBGSystemAudioRow). */
	UPROPERTY(EditAnywhere, Config, Category = "Audio", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGSystemAudioRow"))
	TSoftObjectPtr<UDataTable> AudioTable;

	/** 사용자 설정의 마스터 볼륨이 적용되는 최상위 Sound Class. */
	UPROPERTY(EditAnywhere, Config, Category = "Audio")
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	/** 사용자 볼륨 오버라이드를 담는 Sound Mix. */
	UPROPERTY(EditAnywhere, Config, Category = "Audio")
	TSoftObjectPtr<USoundMix> UserSettingsSoundMix;

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

	// ── 정체 공개 연출 (사망 2단계, BAAM 아트 기획서 "캐릭터의 사망") ──
	//
	// 역할(Role.*)은 4종이지만 시체 외형은 진영 3종뿐이다 — 보안관과 부관은 같은 인간 메시를 쓴다.
	// ABGCharacter 가 RoleReveal 단계에 들어가면 본체 메시를 숨기고 이 메시를 대신 보여준다.

	/** 보안관 / 부관 사망 시 — 해골. */
	UPROPERTY(EditAnywhere, Config, Category = "Reveal")
	TSoftObjectPtr<UStaticMesh> HumanRevealMesh;

	/** 무법자 사망 시 — 마족 시체. */
	UPROPERTY(EditAnywhere, Config, Category = "Reveal")
	TSoftObjectPtr<UStaticMesh> DemonkindRevealMesh;

	/** 배신자 사망 시 — 해골 + 편지. */
	UPROPERTY(EditAnywhere, Config, Category = "Reveal")
	TSoftObjectPtr<UStaticMesh> RenegadeRevealMesh;

	/**
	 * 공개된 역할(Role.*)로 어느 정체 공개 메시를 쓸지 고른다.
	 * 무법자/배신자가 아니면 전부 인간으로 본다 — 보안관을 따로 갈라 쓰지 않는다.
	 */
	UFUNCTION(BlueprintPure, Category = "Reveal")
	TSoftObjectPtr<UStaticMesh> GetRevealMeshForRole(FGameplayTag RoleTag) const;
};

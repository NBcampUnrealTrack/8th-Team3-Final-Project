// BANG! 데이터 서브시스템 — DT 를 로드해 태그로 Row 를 조회한다.
// Team4Project 의 UBaseDataSubsystem 과 동일한 역할.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "Card/BGGameDataTypes.h"
#include "BGDataSubsystem.generated.h"

struct FBGCardInstance;
/**
 * 게임 데이터(DT) 접근 창구. GameInstance 수명과 함께 존재하므로
 * 어디서든 GetGameInstance()->GetSubsystem<UBGDataSubsystem>() 로 접근한다.
 */
UCLASS()
class BG_API UBGDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UBGDataSubsystem();

	// 태그(= Row 이름)로 캐릭터 GAS 구성 Row 를 조회. 없으면 nullptr.
	const FBGCharacterRow* GetCharacterRow(FGameplayTag CharacterTag) const;
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	const FBGCardRow* GetCardRow(FName CardID) const;
	const FBGCardRow* GetCardRow(FGameplayTag CardTag) const;

	/**
	 * 카드 태그(Card.Id.*) → DT Row 이름. 못 찾으면 NAME_None.
	 *
	 * 카드 "종류" 만 아는 자리에서 뷰를 만들 때 쓴다 — 판정 통지(FBGCardResolution)에는
	 * 태그만 실려 오는데 FBGCardInstance 는 Row 이름을 키로 쓰기 때문이다.
	 */
	FName GetCardRowName(FGameplayTag CardTag) const;

	// 모든 카드 확률(가중치) Row 를 모아 반환 (확률 기반 분배용).
	void GetAllCardProbabilities(TArray<FBGCardProbabilityRow>& OutRows) const;
	
	/** DT_BGCard 의 QuantityOfCard 을 읽어 종류별 매수만큼 카드를 전개. InstanceId 는 1부터. */
	void BuildDeck(TArray<FBGCardInstance>& OutDeck) const;

	/**
	 * PvE 전용. DT_BGCard 의 PlayerDeckQuantity/EnemyDeckQuantity 를 Side 에 따라 읽어
	 * 종류별 매수만큼 카드를 전개한다. BuildDeck 과 달리 진영별 매수가 따로 있다.
	 *
	 * EncounterIndex 는 지금은 쓰지 않는다. 항상 DT 의 기준치 그대로 나간다.
	 * TODO: 조우가 진행될수록 매수를 조정하는 난이도 축 스케일링을 넣는다.
	 */
	void BuildPvEDeck(TArray<FBGCardInstance>& OutDeck, EBGDeckSide Side, int32 EncounterIndex = 1) const;

	/**
	 * PvE 조우 승리 보상 후보용. DT_BGCard 에서 파란(장비) Row 전부를 돌려준다.
	 * 플레이어가 그중 하나를 고른다 - 여기서는 후보만 모으고 무작위 선택은 하지 않는다.
	 */
	void GetAllEquipCardIds(TArray<FName>& OutCardIds) const;

	/**
	 * DT_BGCard 의 모든 Row 이름. 이름순으로 정렬해서 준다.
	 *
	 * **덱에 없는 카드를 찾기 위한 것이다.** BuildDeck 결과만 보면 일회용 카드
	 * (`Card.Trait.Ephemeral`)와 매수 0 인 카드가 통째로 시야에서 사라진다 —
	 * `BG_DumpDeck` 이 이 목록과 덱을 대조해 "지급은 되는데 효과가 비어 있는" 카드를 잡는다.
	 */
	void GetAllCardRowNames(TArray<FName>& OutNames) const;

	/**
	 * 장비·상태 FX Row 조회. Key 는 장비 CardId, 상태 태그, 단발 발동 FXRowKey 중 하나다.
	 * 같은 표를 쓰되 지속 오라와 단발 발동은 서로 다른 재생 경로가 맡는다.
	 *
	 * 없으면 nullptr 이고 그게 정상이다. 이펙트를 안 만든 장비·상태가 훨씬 많으므로
	 * 못 찾았다고 경고하지 않는다.
	 */
	const struct FBGStatusFXRow* GetStatusFXRow(FName Key) const;

	/** 카드 연출 정책/전용 연출 Row. Row 이름은 CardId 태그 문자열과 동일하다. */
	const struct FBGCardFXRow* GetCardFXRow(FGameplayTag CardId) const;

	/** 카드에 종속되지 않는 공통 연출 카탈로그. 설정/로드 경로를 호출자에게 노출하지 않는다. */
	const class UBGPresentationDataAsset* GetPresentationData() const { return PresentationData; }

	/** 카드·상태·대상 선택을 포함해 코드에서 관리하는 모든 Niagara에 공통 적용하는 Z 보정값. */
	UFUNCTION(BlueprintPure, Category = "BG|FX")
	float GetGlobalFXSpawnZOffset() const { return GlobalFXSpawnZOffset; }

protected:
	// DT 경로. 에디터에서 이 경로에 DT_BGCharacterRow(FBGCharacterRow) 를 만들어 둔다.
	static const FString CharacterTablePath;

	UPROPERTY()
	TObjectPtr<UDataTable> CharacterTable;

	// 카드 확률 DT 경로. 에디터에서 DT_BGCardProbability(FBGCardProbabilityRow) 를 만들어 둔다.
	static const FString CardProbabilityTablePath;

	UPROPERTY()
	TObjectPtr<UDataTable> CardProbabilityTable;
	
	UPROPERTY()
	TObjectPtr<UDataTable> CardTable;

	//	버프/디버프 이펙트 표. 없어도 된다 — 그때는 이펙트가 안 뜰 뿐이다.
	UPROPERTY()
	TObjectPtr<UDataTable> StatusFXTable;

	UPROPERTY()
	TObjectPtr<UDataTable> CardFXTable;

	UPROPERTY()
	TObjectPtr<class UBGPresentationDataAsset> PresentationData;

	/** 카드·상태 FX가 공유하는 유일한 기본 높이. Row LocationOffset은 개별 보정만 담당한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FX|Transform", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float GlobalFXSpawnZOffset = -20.f;
};

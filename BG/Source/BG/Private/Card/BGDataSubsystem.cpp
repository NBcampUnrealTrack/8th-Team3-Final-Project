#include "Card/BGDataSubsystem.h"

#include "BGGameplayTags.h"                 // Card::Trait::Ephemeral (덱 제외)
#include "Debug/BGCardLog.h"
#include "Card/BGDataSettings.h"
#include "Card/BGCardType.h"
#include "FX/BGStatusFXTypes.h"
#include "FX/BGCardNiagaraTypes.h"
#include "UObject/ConstructorHelpers.h"

const FString UBGDataSubsystem::CharacterTablePath = TEXT("/Game/Data/DT_BGCharacterRow");
const FString UBGDataSubsystem::CardProbabilityTablePath = TEXT("/Game/Data/DT_BGCardProbability");

void UBGDataSubsystem::BuildDeck(TArray<FBGCardInstance>& OutDeck) const
{
	OutDeck.Reset();

	//	Initialize 의 check() 는 Shipping 에서 컴파일 제외된다. 실제 역참조 지점에서 한 번 더 막는다.
	if (!CardTable)
	{
		UE_LOG(LogBGCard, Error,
			TEXT("BuildDeck: CardTable 이 없습니다. Project Settings > Game > BG Data > CardTable 을 확인하세요."));
		return;
	}

	TArray<FName> RowNames = CardTable->GetRowNames();

	//	Row 순서를 명시적으로 고정한다. GetRowNames() 는 DT 내부 순서를 그대로 주므로,
	//	정렬하지 않으면 DT 에 카드 행을 하나 추가하는 것만으로 같은 시드가 다른 판을 만든다.
	//	FastLess() 가 아니라 Compare() 를 쓴다 — FastLess 는 FName 등록 순서에 의존해
	//	실행마다 순서가 달라질 수 있고, 재현성이 무너지면 카드 버그 추적이 불가능해진다.
	RowNames.Sort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });

	int32 NextInstanceId = 1;   // 0 은 "미설정" 과 구분하기 위해 쓰지 않는다
	int32 NumKinds = 0;	//	카드 종류 ( 로그 출력용 )

	for (const FName& RowName : RowNames)
	{
		const FBGCardRow* Row = CardTable->FindRow<FBGCardRow>(RowName, TEXT("BuildDeck"));
		if (!Row || Row->QuantityOfCard <= 0)
		{
			continue;   // QuantityOfCard 0 = 의도적 제외 (아직 미구현 카드 등)
		}

		//	일회용 카드는 덱에서 나오지 않는다 — 규칙이 손패에 직접 만들어 주는 카드다
		//	(GrantCards). 덱에 섞이면 사용 한도를 안 쓰는 카드를 아무나 뽑게 되고,
		//	소비 시 버린 패로 가지 않으므로 판이 갈수록 덱 총량이 줄어든다.
		//
		//	⚠️ QuantityOfCard 를 보지 않고 태그만 본다. 매수를 실수로 적어 넣어도 덱에 안 들어간다 —
		//	   "매수 0 으로 두면 빠진다" 에만 기대면 그 0 이 '일회용이라 0' 인지
		//	   '아직 미구현이라 0' 인지 구분이 사라진다(요정 갑옷이 후자다).
		if (Row->TraitTags.HasTag(BG::Card::Trait::Ephemeral.GetTag()))
		{
			continue;
		}

		for (int32 i = 0; i < Row->QuantityOfCard; ++i)
		{
			FBGCardInstance Card;
			Card.InstanceId = NextInstanceId++;
			Card.CardId = RowName;
			OutDeck.Add(Card);
		}
		++NumKinds;
	}

	UE_LOG(LogBGCard, Log, TEXT("BuildDeck:%d 종류 →%d 장"), NumKinds, OutDeck.Num());
}

void UBGDataSubsystem::BuildPvEDeck(TArray<FBGCardInstance>& OutDeck, EBGDeckSide Side, int32 EncounterIndex) const
{
	//	TODO: EncounterIndex 로 조우별 매수 스케일링을 넣는다. 지금은 안 쓴다.

	OutDeck.Reset();

	if (!CardTable)
	{
		UE_LOG(LogBGCard, Error,
			TEXT("BuildPvEDeck: CardTable 이 없습니다. Project Settings > Game > BG Data > CardTable 을 확인하세요."));
		return;
	}

	TArray<FName> RowNames = CardTable->GetRowNames();
	RowNames.Sort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });

	int32 NextInstanceId = 1;
	int32 NumKinds = 0;

	for (const FName& RowName : RowNames)
	{
		const FBGCardRow* Row = CardTable->FindRow<FBGCardRow>(RowName, TEXT("BuildPvEDeck"));
		const int32 Quantity = Row ? ((Side == EBGDeckSide::Enemy) ? Row->EnemyDeckQuantity : Row->PlayerDeckQuantity) : 0;
		if (!Row || Quantity <= 0)
		{
			continue;
		}

		//	BuildDeck 과 같은 이유로 일회용 카드는 제외한다.
		if (Row->TraitTags.HasTag(BG::Card::Trait::Ephemeral.GetTag()))
		{
			continue;
		}

		for (int32 i = 0; i < Quantity; ++i)
		{
			FBGCardInstance Card;
			Card.InstanceId = NextInstanceId++;
			Card.CardId = RowName;
			Card.Side = Side;
			OutDeck.Add(Card);
		}
		++NumKinds;
	}

	UE_LOG(LogBGCard, Log, TEXT("BuildPvEDeck(Side=%d):%d 종류 →%d 장"), (int32)Side, NumKinds, OutDeck.Num());
}

bool UBGDataSubsystem::GetRandomEquipCardId(FRandomStream& Stream, FName& OutCardId) const
{
	if (!CardTable)
	{
		return false;
	}

	TArray<FName> RowNames = CardTable->GetRowNames();
	RowNames.Sort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });

	TArray<FName> Candidates;
	for (const FName& RowName : RowNames)
	{
		const FBGCardRow* Row = CardTable->FindRow<FBGCardRow>(RowName, TEXT("GetRandomEquipCardId"));
		if (Row && Row->TypeTag == BG::Card::Type::Blue.GetTag())
		{
			Candidates.Add(RowName);
		}
	}

	if (Candidates.IsEmpty())
	{
		return false;
	}

	OutCardId = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	return true;
}

void UBGDataSubsystem::GetAllCardRowNames(TArray<FName>& OutNames) const
{
	OutNames.Reset();
	if (!CardTable)
	{
		return;
	}

	OutNames = CardTable->GetRowNames();

	//	BuildDeck 과 같은 정렬을 쓴다 — 두 목록을 나란히 놓고 보게 되므로 순서가 같아야 한다.
	OutNames.Sort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });
}

UBGDataSubsystem::UBGDataSubsystem()
{
	static ConstructorHelpers::FObjectFinder<UDataTable> DT_Character(*CharacterTablePath);
	if (DT_Character.Succeeded())
	{
		CharacterTable = DT_Character.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> DT_CardProb(*CardProbabilityTablePath);
	if (DT_CardProb.Succeeded())
	{
		CardProbabilityTable = DT_CardProb.Object;
	}
}

const FBGCharacterRow* UBGDataSubsystem::GetCharacterRow(FGameplayTag CharacterTag) const
{
	if (!CharacterTable || !CharacterTag.IsValid())
	{
		return nullptr;
	}

	// bWarnIfRowMissing=false — 경고는 호출자가 맥락에 맞게 남긴다.
	// 이 DT 는 직업(Character.Job.*) Row 만 갖는다. 역할 태그로 조회가 들어오면
	// 못 찾는 게 정상이므로 엔진이 먼저 경고를 찍게 두지 않는다.
	const FName RowName = CharacterTag.GetTagName();
	return CharacterTable->FindRow<FBGCharacterRow>(RowName, TEXT("BGCharacterRow"), /*bWarnIfRowMissing=*/false);
}

void UBGDataSubsystem::GetAllCardProbabilities(TArray<FBGCardProbabilityRow>& OutRows) const
{
	OutRows.Reset();
	if (!CardProbabilityTable)
	{
		return;
	}

	TArray<FBGCardProbabilityRow*> Rows;
	CardProbabilityTable->GetAllRows<FBGCardProbabilityRow>(TEXT("BGCardProbability"), Rows);

	OutRows.Reserve(Rows.Num());
	for (const FBGCardProbabilityRow* Row : Rows)
	{
		if (Row)
		{
			OutRows.Add(*Row);
		}
	}
}

void UBGDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	const UBGDataSettings* Settings = GetDefault<UBGDataSettings>();

	//	이펙트 표는 없어도 된다 — 화면에만 쓰이므로 없으면 이펙트가 안 뜰 뿐이다.
	//	카드 표와 달리 check() 로 막지 않는 이유가 이것이다.
	StatusFXTable = Settings->StatusFXTable.LoadSynchronous();
	CardFXTable = Settings->CardFXTable.LoadSynchronous();

	CardTable = Settings->CardTable.LoadSynchronous();
	check(CardTable != nullptr);	//	BuildDeck: CardTable 이 없습니다 (프로젝트 설정 확인).

	//	check() 는 Shipping 에서 컴파일 제외되므로, 설정 누락 시 여기서 크래시하지 않고 덱 없이 계속한다.
	//	BG_DumpDeck 이 "덱이 비어 있습니다" 로 원인을 알려준다.
	if (!CardTable)
	{
		UE_LOG(LogBGCard, Error,
			TEXT("Initialize: CardTable 로드 실패 — 덱 없이 계속합니다. "
			     "Project Settings > Game > BG Data > CardTable 을 확인하세요."));
		return;
	}
}

const FBGStatusFXRow* UBGDataSubsystem::GetStatusFXRow(FName Key) const
{
	if (!StatusFXTable || Key.IsNone())
	{
		return nullptr;
	}

	//	bWarnIfRowMissing=false — 이펙트를 만들지 않은 장비·상태가 훨씬 많다.
	//	여기서 경고하면 판마다 로그가 도배되고 진짜 경고가 묻힌다.
	return StatusFXTable->FindRow<FBGStatusFXRow>(Key, TEXT("BGStatusFX"), /*bWarnIfRowMissing=*/false);
}

// 카드 이펙트 데이터 테이블에서 참조
const FBGCardFXRow* UBGDataSubsystem::GetCardFXRow(FGameplayTag CardId) const
{
	if (!CardFXTable || !CardId.IsValid())
	{
		return nullptr;
	}

	return CardFXTable->FindRow<FBGCardFXRow>(
		CardId.GetTagName(), TEXT("BGCardFX"), /*bWarnIfRowMissing=*/false);
}

const FBGCardRow* UBGDataSubsystem::GetCardRow(FName CardID) const
{
	const FBGCardRow* Row = CardTable->FindRow<FBGCardRow>(CardID, TEXT("GetCardRow"));
	return Row;
}

const FBGCardRow* UBGDataSubsystem::GetCardRow(FGameplayTag CardTag) const
{
	TArray<FName> RowNames = CardTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FBGCardRow* Row = CardTable->FindRow<FBGCardRow>(RowName, TEXT("GetCardRow"));
		if (Row && Row->CardIdTag == CardTag)
		{
			return Row;
		}
	}
	return nullptr;
}

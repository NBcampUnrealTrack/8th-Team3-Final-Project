#include "Card/BGDataSubsystem.h"

#include "Debug/BGCardLog.h"
#include "Card/BGDataSettings.h"
#include "Card/BGCardType.h"
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

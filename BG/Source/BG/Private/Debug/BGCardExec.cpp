// BANG! 카드/덱 콘솔 테스트 트리거. Network/Session/BGSessionExec.cpp 와 같은 관례:
//   선언은 헤더에 항상 남기고(UFUNCTION(Exec)), 본문만 #if !UE_BUILD_SHIPPING 으로 감싼다.
//
// 호스트 클래스 선택 메모 (Docs/Card-Implementation-Workflow.md §exec 표):
//   덱은 GameState 의 서버 전용 데이터다. GameInstance 에 두면 PIE 의 클라 창에서도 명령이
//   호출되어 "덱이 비어 있습니다" 만 찍히고 원인을 알 수 없다.
//   GameMode 에 두면 World->GetAuthGameMode() 가 클라에서 null 이라 명령이 해석조차 되지 않고
//   "Command not recognized" 가 떠서, 서버 창에서 쳐야 한다는 사실이 즉시 드러난다.

#include "Core/BGGameMode.h"
#include "Core/BGPlayerController.h"

#include "Debug/BGCardLog.h"
#include "Card/BGCardType.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/View/BGCardViewLibrary.h"   // BG_DumpChances — UI 와 같은 값을 찍는다

// BGGameDataTypes.h 는 UGameplayEffect 를 전방 선언만 한다.
// TSubclassOf<UGameplayEffect> 를 bool 로 평가하면 UGameplayEffect::StaticClass() 가 필요하므로
// 완전한 타입이 있어야 한다.
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BGGameplayTags.h"
#include "GAS/Effect/GE_BGDamage.h"

// 덱 총 장수 검증 기준. 0 = 검사하지 않음. 목표 구성이 정해지면 켜서 쓴다.
static int32 GBGExpectedDeckSize = 0;
static FAutoConsoleVariableRef CVarBGExpectedDeckSize(
	TEXT("BG.ExpectedDeckSize"),
	GBGExpectedDeckSize,
	TEXT("BG_DumpDeck 이 검사할 덱 총 장수. 0 이면 검사하지 않는다."),
	ECVF_Default);

void ABGGameMode::BG_DumpDeck()
{
#if !UE_BUILD_SHIPPING
	// 기대 총 장수. 0 이면 검사하지 않는다.
	//   기본판 전사 시절에는 80 이 고정 검증값이었지만, BAAM 은 카드 구성이 다르므로(GDD §6)
	//   목표 장수를 정했을 때만 켜서 쓴다. `Bang.ExpectedDeckSize 64` 처럼 설정.
	const int32 ExpectedDeckSize = GBGExpectedDeckSize;

	const UGameInstance* GI = GetGameInstance();
	const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	if (!Data)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] BGDataSubsystem 없음"));
		return;
	}

	ABGGameState* State = GetGameState<ABGGameState>();
	if (!State)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] ABGGameState 없음"));
		return;
	}

	// Deck 은 Initialize() 에서 한 번 만들어 캐시된다. 여기서 다시 만들지 않으므로
	// 두 번 연속 실행하면 항상 같은 결과가 나와야 한다.
	TArray<FBGCardInstance> Deck;
	State->GetDeck(Deck);
	if (Deck.IsEmpty())
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[Exec] 덱이 비어 있습니다. DT_BGCard 의 QuantityOfCard 가 전부 0 이거나 "
			     "프로젝트 설정(BG Data ▸ CardTable)이 비어 있습니다."));
		return;
	}

	// 종류별 집계 + InstanceId 중복 검사를 한 번에.
	TMap<FName, int32> Counts;
	TSet<int32> SeenIds;
	int32 NumDuplicateIds = 0;

	Counts.Reserve(Deck.Num());
	SeenIds.Reserve(Deck.Num());

	for (const FBGCardInstance& Card : Deck)
	{
		++Counts.FindOrAdd(Card.CardId);

		bool bAlreadySeen = false;
		SeenIds.Add(Card.InstanceId, &bAlreadySeen);
		if (bAlreadySeen)
		{
			++NumDuplicateIds;
		}
	}

	Counts.KeySort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });

	UE_LOG(LogBGCard, Log, TEXT("===== BG_DumpDeck ====="));

	//	"이 카드가 동작하는가" 의 기준은 카드 타입마다 다르다. 둘은 겹치지 않는다.
	//	  갈색 : OutcomeEffects 가 있는가 (UGA_BGCardEffects 가 이것만 읽는다)
	//	  파란 : EquipEffect 가 있는가   (판정 없이 장착 경로로 빠진다)
	//
	//	⚠️ AbilityEventTag 로는 판단하지 않는다. 카드 → GA 라우팅이 PlayerController 의
	//	   AbilityByCardId 로 옮겨간 뒤 이 태그는 페이로드 로그용으로만 남았고, DT 에서
	//	   비어 있는 것이 정상이다. 예전 기준을 그대로 두면 정상 카드 전부가 "미구현" 으로
	//	   신고되어, 진짜 문제가 생겼을 때 경고에 묻힌다.
	int32 NumNoEquipEffect = 0;
	int32 NumNoEffectData = 0;
	int32 NumCinematic = 0;
	for (const TPair<FName, int32>& Pair : Counts)
	{
		const FBGCardRow* Row = Data->GetCardRow(Pair.Key);
		const bool bIsEquip = (Row != nullptr) && Row->TypeTag == BG::Card::Type::Blue.GetTag();

		//	장비인데 걸어줄 GE 가 없다 — 장착은 되지만 아무 보정도 걸리지 않는다.
		const bool bNoEquipEffect = (Row != nullptr) && bIsEquip && !Row->EquipEffect;
		if (bNoEquipEffect)
		{
			++NumNoEquipEffect;
		}

		//	효과 데이터가 비어 있는지 검사한다.
		//	이게 없으면 "판정도 정상, GA 발동도 정상, 카드 소비도 정상인데 효과만 0" 인
		//	조용한 실패가 된다 — 뱅의 피해가 안 들어가는 문제로 실제로 한 번 겪었다.
		//	⚠️ 효과 목록(OutcomeEffects)만 본다. 예전에는 등급별 수치 필드도 함께 봐서
		//	   거기 값이 남아 있으면 이 검사가 통째로 무력화됐다 — 그 필드는 지웠다.
		const bool bNoEffectData = (Row != nullptr) && !bIsEquip && Row->OutcomeEffects.IsEmpty();
		if (bNoEffectData)
		{
			++NumNoEffectData;
		}

		//	연출 카드 표시. 경고가 아니라 상태다 — 태그를 넣었는데 안 붙었는지,
		//	넣지 않았는데 붙었는지를 DT 를 열지 않고 확인하려는 것이다.
		const bool bCinematic =
			(Row != nullptr) && Row->TraitTags.HasTag(BG::Card::Trait::Cinematic.GetTag());
		if (bCinematic)
		{
			++NumCinematic;
		}

		UE_LOG(LogBGCard, Log, TEXT("  %-32s %3d 장%s%s"),
			*Pair.Key.ToString(),
			Pair.Value,
			bCinematic ? TEXT("  [연출]") : TEXT("      "),
			(Row == nullptr)  ? TEXT("   [!! DT_BGCard 에 Row 없음]") :
			bNoEquipEffect    ? TEXT("   [!! EquipEffect 없음 — 장착해도 효과 없음]") :
			bNoEffectData     ? TEXT("   [!! 효과 데이터 없음 — OutcomeEffects 를 채우세요]") :
			                    TEXT(""));
	}

	if (ExpectedDeckSize > 0)
	{
		UE_LOG(LogBGCard, Log, TEXT("총 %d 종류 / %d 장   (기대 %d : %s)"),
			Counts.Num(), Deck.Num(), ExpectedDeckSize,
			(Deck.Num() == ExpectedDeckSize) ? TEXT("일치") : TEXT("불일치 ← QuantityOfCard 확인"));
	}
	else
	{
		UE_LOG(LogBGCard, Log, TEXT("총 %d 종류 / %d 장   (기대 장수 미설정 — Bang.ExpectedDeckSize 로 켤 수 있음)"),
			Counts.Num(), Deck.Num());
	}

	UE_LOG(LogBGCard, Log, TEXT("연출 카드 %d 종 (Card.Trait.Cinematic)"), NumCinematic);

	if (NumNoEquipEffect > 0)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("EquipEffect 가 없는 장비 %d 종이 덱에 있습니다 (장착은 되지만 보정이 걸리지 않음). "
			     "구현 전까지 QuantityOfCard 를 0 으로 두면 구현된 카드만으로 테스트할 수 있습니다."),
			NumNoEquipEffect);
	}

	if (NumNoEffectData > 0)
	{
		UE_LOG(LogBGCard, Error,
			TEXT("효과 데이터가 빈 카드 %d 종이 덱에 있습니다. 사용해도 효과가 0 이지만 "
			     "판정·GA 발동·카드 소비는 정상 동작하므로 원인을 찾기 어렵습니다. "
			     "DT_BGCard 의 OutcomeEffects 를 채우세요."),
			NumNoEffectData);
	}

	if (NumDuplicateIds > 0)
	{
		// 중복되면 손패에서 카드를 제거할 때 엉뚱한 카드가 지워진다. 반드시 잡고 넘어갈 것.
		UE_LOG(LogBGCard, Error,
			TEXT("InstanceId 중복 %d 건 — BuildDeck 의 NextInstanceId 카운터를 확인하세요."),
			NumDuplicateIds);
	}

	// ── 덱 밖 카드 ────────────────────────────────────────────────────
	//
	//	위 집계는 **덱**을 돈다. 덱에 없는 카드는 한 줄도 나오지 않는다.
	//	그런데 일회용 카드(Card.Trait.Ephemeral)는 덱에 없으면서 실제로 손에 들어오고 사용된다 —
	//	즉 **가장 중요한 검사인 "효과 데이터가 비었는가" 를 통째로 빠져나간다.**
	//	그 실패 모양은 위 NumNoEffectData 경고문이 설명하는 것과 똑같다(판정·발동·소비 전부 정상,
	//	효과만 0). 이 구획이 없으면 일회용 카드를 늘릴수록 검증 사각지대가 커진다.
	//
	//	매수 0 인 카드는 함께 나열하되 효과는 따지지 않는다 — 아직 안 만든 카드라는 뜻이라
	//	경고를 내면 노이즈가 되고, 진짜 문제가 거기 묻힌다(§1.1 이 겪은 오탐과 같은 구조).
	{
		TArray<FName> AllRows;
		Data->GetAllCardRowNames(AllRows);

		int32 NumEphemeralNoEffect = 0;
		int32 NumOutOfDeck = 0;

		for (const FName& RowName : AllRows)
		{
			if (Counts.Contains(RowName))
			{
				continue;   // 덱에 있다 — 위에서 이미 검사했다
			}

			const FBGCardRow* Row = Data->GetCardRow(RowName);
			if (!Row)
			{
				continue;
			}

			++NumOutOfDeck;

			const bool bEphemeral = Row->TraitTags.HasTag(BG::Card::Trait::Ephemeral.GetTag());
			const bool bIsEquip   = Row->TypeTag == BG::Card::Type::Blue.GetTag();

			//	일회용만 효과를 따진다. 갈색은 OutcomeEffects, 파란(장비)은 EquipEffect 다.
			const bool bNoEffect = bEphemeral &&
				(bIsEquip ? (Row->EquipEffect == nullptr) : Row->OutcomeEffects.IsEmpty());
			if (bNoEffect)
			{
				++NumEphemeralNoEffect;
			}

			UE_LOG(LogBGCard, Log, TEXT("  %-32s   -  %s%s"),
				*RowName.ToString(),
				bEphemeral ? TEXT("[일회용] 덱 제외") : TEXT("[매수 0] 덱 제외"),
				bNoEffect ? TEXT("   [!! 효과 데이터 없음 — 지급돼도 아무 일도 일어나지 않음]")
				          : TEXT(""));
		}

		UE_LOG(LogBGCard, Log, TEXT("덱 밖 카드 %d 종"), NumOutOfDeck);

		if (NumEphemeralNoEffect > 0)
		{
			UE_LOG(LogBGCard, Error,
				TEXT("효과 데이터가 빈 일회용 카드 %d 종. 손패에는 들어오고 사용도 되지만 "
				     "아무 일도 일어나지 않습니다 — 덱에 없어 위 집계에 잡히지 않으므로 "
				     "이 줄이 유일한 경고입니다."),
				NumEphemeralNoEffect);
		}
	}
#endif // !UE_BUILD_SHIPPING
}

void ABGGameMode::BG_PrintDeck()
{
#if !UE_BUILD_SHIPPING

	ABGGameState* State = GetGameState<ABGGameState>();
	if (!State)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] ABGGameState 없음"));
		return;
	}

	// 셔플된 실제 순서. DrawFromDeck 이 0 번부터 뽑으므로 이 출력의 위에서부터 나온다.
	TArray<FBGCardInstance> Deck;
	State->GetDeck(Deck);
	if (Deck.IsEmpty())
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[Exec] 덱이 비어 있습니다. DT_BGCard 의 QuantityOfCard 가 전부 0 이거나 "
				 "프로젝트 설정(BG Data ▸ CardTable)이 비어 있습니다."));
		return;
	}
	
	for (const FBGCardInstance& Card : Deck)
	{
		FName name = Card.CardId;
		FString names = name.ToString();
		int32 num = Card.InstanceId;
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] DeckList Number : %d, Name : %s"), num, *names);
	}
#endif // !UE_BUILD_SHIPPING
}

// ======================================================================================
//  2.7 — 손패 배분 / 조회
//
//  두 명령의 호스트 클래스가 다른 것이 요점이다:
//    BG_DealHand : 덱을 소비하는 서버 권위 조작 → GameMode (클라 창에서는 해석되지 않음)
//    BG_DumpHand : 창마다 자기 손패를 확인 → PlayerController (각 클라에서 따로 실행)
// ======================================================================================

void ABGGameMode::BG_DealHand(int32 Count)
{
#if !UE_BUILD_SHIPPING
	//	exec 는 생략된 뒤쪽 인자를 0 으로 넘긴다. C++ 기본값에 기대지 말 것.
	if (Count <= 0)
	{
		Count = 5;
	}

	ABGGameState* State = GetGameState<ABGGameState>();
	if (!State)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] ABGGameState 없음"));
		return;
	}

	TArray<APlayerController*> Players;
	CollectPlayers(Players);
	if (Players.IsEmpty())
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] 접속한 플레이어가 없습니다."));
		return;
	}

	UE_LOG(LogBGCard, Log, TEXT("===== BG_DealHand (%d 명 × %d 장) ====="), Players.Num(), Count);

	for (APlayerController* PC : Players)
	{
		ABGPlayerState* PS = PC ? PC->GetPlayerState<ABGPlayerState>() : nullptr;
		if (!PS)
		{
			UE_LOG(LogBGCard, Warning, TEXT("  %s : ABGPlayerState 없음 — 스킵"),
				PC ? *PC->GetName() : TEXT("null"));
			continue;
		}

		const int32 Before = PS->GetHandCount();

		//	덱 뽑기는 반드시 이 함수를 거친다 — 손패 상한 검사가 여기 있다.
		//	테스트 명령이 상한을 우회하면 상한 자체를 검증할 수 없다.
		const int32 Dealt = State->DrawCardsToHand(PS, Count);

		UE_LOG(LogBGCard, Log, TEXT("  좌석 %d (%s) → %d/%d 장 (손패 %d → %d, 상한 %d)"),
			PS->GetSeatIndex(), *PC->GetName(), Dealt, Count,
			Before, PS->GetHandCount(), State->GetHandLimitForSeat(PS->GetSeatIndex()));
	}

	UE_LOG(LogBGCard, Log, TEXT("배분 후 — 덱 %d 장 / 버린 패 %d 장"),
		State->GetDeckCount(), State->GetDiscardCount());
#endif // !UE_BUILD_SHIPPING
}

void ABGPlayerController::BG_DumpHand()
{
#if !UE_BUILD_SHIPPING
	const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	if (!PS)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] ABGPlayerState 없음"));
		return;
	}

	const bool bIsServer = HasAuthority();
	const TArray<FBGCardInstance>& Hand = PS->GetHand();

	UE_LOG(LogBGCard, Log, TEXT("===== BG_DumpHand — 좌석 %d (%s) ====="),
		PS->GetSeatIndex(), bIsServer ? TEXT("서버") : TEXT("클라"));

	for (const FBGCardInstance& Card : Hand)
	{
		UE_LOG(LogBGCard, Log, TEXT("  #%-3d %s"), Card.InstanceId, *Card.CardId.ToString());
	}

	//	Hand.Num() 과 HandCount 가 어긋나면 손패를 Add/RemoveCardFromHand 를 거치지 않고 건드린 것이다.
	UE_LOG(LogBGCard, Log, TEXT("내 손패 %d 장 (HandCount=%d%s)"),
		Hand.Num(), PS->GetHandCount(),
		(Hand.Num() == PS->GetHandCount()) ? TEXT("") : TEXT(" ← 불일치!"));

	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	//	이 값이 클라에서 0 이면 GetLifetimeReplicatedProps 등록이 빠진 것이다.
	UE_LOG(LogBGCard, Log, TEXT("덱 %d 장 / 버린 패 %d 장"),
		GS->GetDeckCount(), GS->GetDiscardCount());

	//	── 은닉 정보 검증 ──
	//	클라에서는 남의 Hand 배열이 반드시 비어 있어야 한다(COND_OwnerOnly).
	//	서버(리슨 호스트)는 모든 PlayerState 를 그대로 들고 있으므로 차 있는 것이 정상이다.
	for (APlayerState* Other : GS->PlayerArray)
	{
		const ABGPlayerState* OtherPS = Cast<ABGPlayerState>(Other);
		if (!OtherPS || OtherPS == PS)
		{
			continue;
		}

		const int32 VisibleCards = OtherPS->GetHand().Num();
		const bool bLeak = !bIsServer && VisibleCards > 0;

		UE_LOG(LogBGCard, Log, TEXT("  타 좌석 %d : 공개 장수 %d, 내용 %d 개 보임%s"),
			OtherPS->GetSeatIndex(), OtherPS->GetHandCount(), VisibleCards,
			bLeak ? TEXT("  ← 은닉 정보 누출! COND_OwnerOnly 확인") :
			bIsServer ? TEXT("  (서버라 정상)") : TEXT(""));
	}
#endif // !UE_BUILD_SHIPPING
}

void ABGPlayerController::BG_DumpChances()
{
#if !UE_BUILD_SHIPPING
	const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	if (!PS)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] ABGPlayerState 없음"));
		return;
	}

	//	MakeHandViews 가 UI 에 넘기는 것과 똑같은 값을 찍는다.
	//	여기서 계산을 따로 하면 "콘솔은 맞는데 화면은 틀린" 상황을 잡아낼 수 없다.
	const TArray<FBGCardView> Views = UBGCardViewLibrary::MakeHandViews(PS);

	const UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn());

	UE_LOG(LogBGCard, Log, TEXT("===== BG_DumpChances — 좌석 %d (%s) ====="),
		PS->GetSeatIndex(), HasAuthority() ? TEXT("서버") : TEXT("클라"));

	if (!ASC)
	{
		//	ASC 가 없으면 보정을 못 읽어 기본 확률만 나온다. 조용히 넘어가면
		//	"장비를 꼈는데 확률이 그대로" 를 UI 버그로 오해하게 된다.
		UE_LOG(LogBGCard, Warning,
			TEXT("  ASC 없음(폰 미스폰?) — 보정을 읽지 못해 기본 확률만 표시됩니다."));
	}

	int32 NumWithChance = 0;

	for (const FBGCardView& View : Views)
	{
		if (!View.bHasChances)
		{
			UE_LOG(LogBGCard, Log, TEXT("  #%-3d %-24s 판정 없음(장비)"),
				View.InstanceId, *View.CardId.ToString());
			continue;
		}

		++NumWithChance;

		const FBGCardChanceView& B = View.BaseChance;
		const FBGCardChanceView& F = View.FinalChance;

		UE_LOG(LogBGCard, Log,
			TEXT("  #%-3d %-24s 기본 [대실%5.1f 실%5.1f 성%5.1f 대성%5.1f] → 최종 [대실%5.1f 실%5.1f 성%5.1f 대성%5.1f]  성공이상 %.1f%%%s"),
			View.InstanceId, *View.CardId.ToString(),
			B.CriticalFailure, B.Failure, B.Success, B.CriticalSuccess,
			F.CriticalFailure, F.Failure, F.Success, F.CriticalSuccess,
			F.GetSuccessOrBetter(),
			View.bChanceModified ? TEXT("  ← 보정됨") : TEXT(""));
	}

	UE_LOG(LogBGCard, Log, TEXT("판정 카드 %d 장 / 손패 %d 장"), NumWithChance, Views.Num());
#endif // !UE_BUILD_SHIPPING
}

// ======================================================================================
//  턴 — 조회는 창마다 자기 기준이므로 PlayerController 에 둔다.
// ======================================================================================

void ABGPlayerController::BG_EndTurn()
{
#if !UE_BUILD_SHIPPING
	RequestEndTurn();
#endif
}

void ABGPlayerController::BG_SkipIntro()
{
#if !UE_BUILD_SHIPPING
	//	단계는 서버가 소유한다 — 클라가 혼자 건너뛰면 그 화면만 어긋난다.
	ServerRequestSkipIntro();
#endif
}

void ABGPlayerController::BG_DumpTurn()
{
#if !UE_BUILD_SHIPPING
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] ABGGameState 없음"));
		return;
	}

	const int32 MySeat = GetMySeatIndex();

	UE_LOG(LogBGCard, Log, TEXT("===== BG_DumpTurn ====="));
	UE_LOG(LogBGCard, Log, TEXT("  현재 턴 좌석 : %d   페이즈 : %s"),
		GS->GetCurrentSeat(), *GS->GetPhaseTag().ToString());
	UE_LOG(LogBGCard, Log, TEXT("  내 좌석 : %d   낼 수 있나 : %s   버려야 하나 : %s"),
		MySeat,
		IsMyTurnToPlay()    ? TEXT("예") : TEXT("아니오"),
		IsMyTurnToDiscard() ? TEXT("예") : TEXT("아니오"));

	if (const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>())
	{
		UE_LOG(LogBGCard, Log, TEXT("  손패 %d 장 / 한도 %d (한도 = 현재 Health)"),
			PS->GetHandCount(), GS->GetHandLimitForSeat(MySeat));
		UE_LOG(LogBGCard, Log, TEXT("  카드 사용 %d / %d  (남음 : %s)"),
			PS->GetCardsUsedThisTurn(), GS->GetCardUseLimitForSeat(MySeat),
			GS->HasCardUsesLeft(MySeat) ? TEXT("예") : TEXT("아니오"));
	}

	const TArray<int32> Alive = GS->GetAliveSeatsInTableOrder();
	FString AliveStr;
	for (const int32 Seat : Alive)
	{
		AliveStr += FString::Printf(TEXT("%d(HP%d) "), Seat, GS->GetSeatHealth(Seat));
	}
	UE_LOG(LogBGCard, Log, TEXT("  살아있는 좌석 : %s"), AliveStr.IsEmpty() ? TEXT("(없음)") : *AliveStr);
#endif
}

// ======================================================================================
//  사망 / 승패 테스트 — 서버 권위이므로 GameMode 에 둔다.
// ======================================================================================

void ABGGameMode::BG_Damage(int32 Seat, int32 Amount)
{
#if !UE_BUILD_SHIPPING
	ABGGameState* GS = GetGameState<ABGGameState>();
	if (!GS)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] ABGGameState 없음"));
		return;
	}

	const ABGPlayerState* PS = GS->GetPlayerStateBySeat(Seat);
	UAbilitySystemComponent* ASC =
		PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn()) : nullptr;
	if (!ASC)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] 좌석 %d 의 ASC 를 찾지 못했습니다."), Seat);
		return;
	}

	// exec 는 생략된 인자를 0 으로 넘긴다. 0 이면 즉사시킨다(승패 판정 테스트 편의).
	const int32 Damage = (Amount > 0) ? Amount : FMath::Max(1, GS->GetSeatHealth(Seat));

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_BGDamage::StaticClass(), 1.f, Context);
	if (Spec.IsValid())
	{
		// 피해는 음수로 넣는다(Health 가산 = 감소).
		Spec.Data->SetSetByCallerMagnitude(BG::SetByCaller::Damage.GetTag(), -static_cast<float>(Damage));
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	UE_LOG(LogBGCard, Log, TEXT("[Exec] 좌석 %d 에 피해 %d — 남은 HP %d"),
		Seat, Damage, GS->GetSeatHealth(Seat));
#endif
}

void ABGGameMode::BG_DamageAllOutlaws(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	ABGGameState* GS = GetGameState<ABGGameState>();
	if (!GS)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] ABGGameState 없음"));
		return;
	}

	// exec 는 생략된 인자를 0 으로 넘긴다.
	const int32 Damage = (Amount > 0) ? Amount : 1;

	int32 NumHit = 0;
	for (const int32 Seat : GS->GetAliveSeatsInTableOrder())
	{
		if (GS->GetSeatRole(Seat) != BG::Role::Outlaw.GetTag())
		{
			continue;
		}

		const ABGPlayerState* PS = GS->GetPlayerStateBySeat(Seat);
		UAbilitySystemComponent* ASC =
			PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn()) : nullptr;
		if (!ASC)
		{
			UE_LOG(LogBGCard, Warning, TEXT("[Exec] 좌석 %d 의 ASC 를 찾지 못했습니다."), Seat);
			continue;
		}

		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_BGDamage::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			// 피해는 음수로 넣는다(Health 가산 = 감소).
			Spec.Data->SetSetByCallerMagnitude(BG::SetByCaller::Damage.GetTag(), -static_cast<float>(Damage));
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}

		++NumHit;
	}

	UE_LOG(LogBGCard, Log, TEXT("[Exec] 무법자 %d 명에게 피해 %d 적용"), NumHit, Damage);
#endif
}

void ABGGameMode::BG_ForceWin(const FString& Side)
{
#if !UE_BUILD_SHIPPING
	ABGGameState* GS = GetGameState<ABGGameState>();
	if (!GS)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] ABGGameState 없음"));
		return;
	}

	FGameplayTag Winner;
	if      (Side.Equals(TEXT("Outlaw"),   ESearchCase::IgnoreCase)) { Winner = BG::Role::Outlaw.GetTag(); }
	else if (Side.Equals(TEXT("Sheriff"),  ESearchCase::IgnoreCase)) { Winner = BG::Role::Sheriff.GetTag(); }
	else if (Side.Equals(TEXT("Renegade"), ESearchCase::IgnoreCase)) { Winner = BG::Role::Renegade.GetTag(); }
	else
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] BG_ForceWin — 알 수 없는 진영 '%s' (Outlaw / Sheriff / Renegade)"), *Side);
		return;
	}

	GS->DebugForceWin(Winner);
#endif
}

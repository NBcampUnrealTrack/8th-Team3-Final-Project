#include "Core/BGGameMode.h"
#include "BGGameplayTags.h"
#include "Lobby/BGMatchStartComponent.h"
#include "Card/BGDataSubsystem.h"
#include "Core/BGPlayerState.h"
#include "Debug/BGNetLog.h"
#include "Core/BGCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Core/BGGameState.h"

ABGGameMode::ABGGameMode()
{
	// 좌석/손패 등 플레이어별 데이터의 소유자(md §1.2).
	PlayerStateClass = ABGPlayerState::StaticClass();
	GameStateClass = ABGGameState::StaticClass();
	
	// 판 시작 진행은 컴포넌트가 맡는다.
	MatchStart = CreateDefaultSubobject<UBGMatchStartComponent>(TEXT("MatchStart"));
}

void ABGGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	// 입장 가부 판정은 진행 컴포넌트가 갖는다 — 여기는 엔진 훅을 넘겨주기만 한다.
	if (MatchStart && !MatchStart->CanAcceptPlayer(ErrorMessage))
	{
		UE_LOG(LogBGNet, Log, TEXT("[MatchStart] 접속 거절(%s) — %s"), *Address, *ErrorMessage);
	}
}

void ABGGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 정원이 차면 컴포넌트가 알아서 판을 연다(md §1.4 — 로비에서 대기하다 전환).
	if (MatchStart)
	{
		MatchStart->NotifyPlayerJoined();
	}
}

TArray<FGameplayTag> ABGGameMode::BuildRolePool(int32 NumPlayers)
{
	using namespace BG::Role;

	TArray<FGameplayTag> Pool;

	// 공통: 보안관 1, 배신자 1.
	Pool.Add(Sheriff.GetTag());
	Pool.Add(Renegade.GetTag());

	// 무법자: 4~5인 2명 / 6~7인 3명.
	const int32 NumOutlaws = (NumPlayers <= 5) ? 2 : 3;
	for (int32 i = 0; i < NumOutlaws; ++i)
	{
		Pool.Add(Outlaw.GetTag());
	}

	// 부관: 남는 자리를 채운다 → 4인 0 / 5·6인 1 / 7인 2.
	const int32 NumDeputies = NumPlayers - Pool.Num();
	for (int32 i = 0; i < NumDeputies; ++i)
	{
		Pool.Add(Deputy.GetTag());
	}

	return Pool;
}

void ABGGameMode::CollectPlayers(TArray<APlayerController*>& OutPlayers) const
{
	OutPlayers.Reset();

	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				OutPlayers.Add(PC);
			}
		}
	}
}

void ABGGameMode::AssignRoles()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<APlayerController*> Players;
	CollectPlayers(Players);

	const int32 Num = Players.Num();
	if (Num < MinPlayers || Num > MaxPlayers)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BG] AssignRoles: 접속 인원 %d 명은 지원 범위(%d~%d) 밖 — 배정 취소."),
			Num, MinPlayers, MaxPlayers);
		return;
	}

	TArray<FGameplayTag> Pool = BuildRolePool(Num);
	if (Pool.Num() != Num)
	{
		// 인원수와 풀 크기가 어긋나면(로직 오류) 잘못된 배정을 막는다.
		UE_LOG(LogTemp, Error, TEXT("[BG] AssignRoles: 역할 풀(%d)과 인원(%d) 불일치."),
			Pool.Num(), Num);
		return;
	}

	// Fisher-Yates 셔플 (뒤에서부터 랜덤 교환).
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Pool.Swap(i, j);
	}

	// 셔플된 풀에서 한 명씩 태그를 빼서 배정.
	// SetCharacterTag 가 캐릭터의 ASC 에 DT Row(스탯 GE/패시브/어빌리티)를 부여한다.
	for (int32 i = 0; i < Num; ++i)
	{
		APlayerController* PC = Players[i];

		// 좌석은 거리 계산·대상 지정의 기준이라 역할과 함께 확정한다.
		if (ABGPlayerState* PS = PC ? PC->GetPlayerState<ABGPlayerState>() : nullptr)
		{
			PS->SetSeatIndex(i);
		}

		ABGCharacter* Char = PC ? Cast<ABGCharacter>(PC->GetPawn()) : nullptr;
		if (!Char)
		{
			// 아직 폰이 스폰되지 않은 플레이어 — 스킵 (로그로 남겨 추적).
			UE_LOG(LogTemp, Warning, TEXT("[BG] AssignRoles: %s 의 캐릭터 없음 — 배정 스킵."),
				PC ? *PC->GetName() : TEXT("null"));
			continue;
		}

		const FGameplayTag RoleTag = Pool[i];
		Char->SetCharacterTag(RoleTag);

		// 보안관만 공개 역할이다. 나머지는 사망하거나 판이 끝날 때까지 감춘다.
		// (CharacterTag 자체는 COND_OwnerOnly 라 본인 외에는 복제되지 않는다)
		if (ABGPlayerState* PS = PC->GetPlayerState<ABGPlayerState>())
		{
			if (RoleTag == BG::Role::Sheriff.GetTag())
			{
				PS->SetPublicRole(RoleTag);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[BG] AssignRoles: %s → %s"),
			*PC->GetName(), *RoleTag.ToString());
	}
}

// ======================================================================================
//  [현재 미사용] 확률 기반 카드 분배
//
//  실제 분배 경로는 ABGGameState::DrawFromDeck + ABGPlayerState::AddCardToHand 다.
//  아래는 복원추출(가중치) 방식이라 InstanceId 가 없고 같은 카드가 무한히 나올 수 있어
//  덱 기반 시스템과 섞이면 안 된다. 참고용으로만 남긴다 — 호출처 없음.
// ======================================================================================
FGameplayTag ABGGameMode::DrawWeightedCard() const
{
	const UGameInstance* GI = GetGameInstance();
	const UBGDataSubsystem* DS = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	if (!DS)
	{
		return FGameplayTag();
	}

	TArray<FBGCardProbabilityRow> Rows;
	DS->GetAllCardProbabilities(Rows);
	if (Rows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] DrawWeightedCard: 카드 확률 데이터 없음(DT 비었거나 미지정)."));
		return FGameplayTag();
	}

	// 가중치 합 (음수 방지).
	float Total = 0.f;
	for (const FBGCardProbabilityRow& Row : Rows)
	{
		Total += FMath::Max(0.f, Row.Weight);
	}
	if (Total <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] DrawWeightedCard: 가중치 합이 0 — 뽑을 카드 없음."));
		return FGameplayTag();
	}

	// [0, Total) 난수를 누적 가중치 구간에 대응시켜 선택.
	const float Pick = FMath::FRandRange(0.f, Total);
	float Cumulative = 0.f;
	for (const FBGCardProbabilityRow& Row : Rows)
	{
		Cumulative += FMath::Max(0.f, Row.Weight);
		if (Pick < Cumulative)
		{
			return Row.CardId;
		}
	}

	return Rows.Last().CardId; // 부동소수 오차 보정 — 마지막 카드로.
}

void ABGGameMode::DealCards(int32 CardsPerPlayer)
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<APlayerController*> Players;
	CollectPlayers(Players);

	for (APlayerController* PC : Players)
	{
		TArray<FString> Dealt;
		Dealt.Reserve(CardsPerPlayer);

		for (int32 i = 0; i < CardsPerPlayer; ++i)
		{
			const FGameplayTag Card = DrawWeightedCard();
			Dealt.Add(Card.IsValid() ? Card.ToString() : TEXT("(없음)"));

			// TODO(손패 시스템): 여기서 뽑은 Card 를 이 플레이어의 PlayerState.Hand 에 추가.
		}

		UE_LOG(LogTemp, Log, TEXT("[BG] 카드 분배 — %s: %s"),
			PC ? *PC->GetName() : TEXT("null"), *FString::Join(Dealt, TEXT(", ")));
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/BGGameState.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/GameInstance.h"
#include "Debug/BGCardLog.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGDiceComponent.h"
#include "BGGameplayTags.h"
#include "Core/BGPlayerState.h"
#include "Core/BGCharacter.h"
#include "GAS/BGAttributeSet.h"
#include "Core/BGGameInstance.h"
#include "Lobby/BGRopeComponent.h"
#include "Lobby/BGSeatComponent.h"
#include "GAS/Effect/GE_BGDamage.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ABGGameState::ABGGameState()
{
	//	확률 판정은 컴포넌트가 맡는다 (GameMode 가 아니라 여기 — 클라도 설정을 읽어야 하고
	//	시드 난수의 소유자가 GameState 이기 때문).
	Dice = CreateDefaultSubobject<UBGDiceComponent>(TEXT("Dice"));

	//	턴 제한 시간도 같은 이유로 여기 — 남은 시간을 클라 UI 가 읽어야 한다.
	Rope = CreateDefaultSubobject<UBGRopeComponent>(TEXT("Rope"));
}

void ABGGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ABGGameState, DeckCount);
	DOREPLIFETIME(ABGGameState, DiscardCount);
	DOREPLIFETIME(ABGGameState, DiscardTop);
	DOREPLIFETIME(ABGGameState, PhaseTag);
	DOREPLIFETIME(ABGGameState, CurrentSeat);
	DOREPLIFETIME(ABGGameState, WinningRoleTag);
	DOREPLIFETIME(ABGGameState, bMatchRunning);
	DOREPLIFETIME(ABGGameState, MatchEndStage);
	DOREPLIFETIME(ABGGameState, CameraShot);
	DOREPLIFETIME(ABGGameState, CinematicStage);
	DOREPLIFETIME(ABGGameState, CinematicInstigator);
	DOREPLIFETIME(ABGGameState, CinematicTarget);
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  카드 연출
//
//  Phase.* 와 별개의 축이다. PhaseTag 는 연출 중에도 그대로 남아 있고, 연출이 끝나면
//  단계를 None 으로 되돌리는 것만으로 원래 상태가 복구된다.
// ══════════════════════════════════════════════════════════════════════════════════════

ABGCharacter* ABGGameState::GetCinematicSubject() const
{
	switch (CinematicStage)
	{
	case EBGCinematicStage::Primary:   return CinematicInstigator;
	//	피해자가 없는 카드(회복 등)는 사용자를 계속 비춘다 — 빈 화면을 3초 보여주는 것보다 낫다.
	case EBGCinematicStage::Secondary: return CinematicTarget ? CinematicTarget : CinematicInstigator;
	default:                           return nullptr;
	}
}

void ABGGameState::BeginCardCinematic(ABGCharacter* Caster, ABGCharacter* Victim)
{
	if (!HasAuthority())
	{
		return;
	}

	//	연출 중에 또 시작하면 단계가 꼬인다. 카드 사용은 연출 중 막혀 있지만(CanSeatPlayCards),
	//	서버 내부에서 연쇄로 불릴 여지가 있어 여기서도 한 번 막는다.
	if (IsCinematicPlaying())
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[Cinematic] 이미 연출 중입니다(%d 단계) — 새 연출 요청을 무시합니다."),
			static_cast<int32>(CinematicStage));
		return;
	}

	if (!Caster)
	{
		//	주인공이 없으면 비출 대상이 없다. 상태만 걸어 두면 입력만 막히고 화면은 그대로다.
		UE_LOG(LogBGCard, Warning, TEXT("[Cinematic] 사용자 캐릭터가 없어 연출을 건너뜁니다."));
		return;
	}

	CinematicInstigator = Caster;
	CinematicTarget     = Victim;

	SetCinematicStage(EBGCinematicStage::Primary);
}

void ABGGameState::SetCinematicStage(EBGCinematicStage NewStage)
{
	if (!HasAuthority() || CinematicStage == NewStage)
	{
		return;
	}

	CinematicStage = NewStage;

	//	서버는 자기 OnRep 을 받지 못한다 — 여기서 직접 부르지 않으면 리슨서버 호스트만 연출이 안 뜬다.
	HandleCinematicStage();

	GetWorldTimerManager().ClearTimer(CinematicStageTimer);

	//	다음 단계 예약. 길이가 0 이하면 그 단계를 건너뛴다 — 연출을 끄고 규칙만 돌릴 때 쓴다.
	switch (NewStage)
	{
	case EBGCinematicStage::Primary:
		if (PrimaryCinematicSeconds > 0.f)
		{
			GetWorldTimerManager().SetTimer(CinematicStageTimer,
				[this]() { SetCinematicStage(EBGCinematicStage::Secondary); },
				PrimaryCinematicSeconds, /*bLoop=*/false);
		}
		else
		{
			SetCinematicStage(EBGCinematicStage::Secondary);
		}
		break;

	case EBGCinematicStage::Secondary:
		if (SecondaryCinematicSeconds > 0.f)
		{
			GetWorldTimerManager().SetTimer(CinematicStageTimer,
				[this]() { SetCinematicStage(EBGCinematicStage::None); },
				SecondaryCinematicSeconds, /*bLoop=*/false);
		}
		else
		{
			SetCinematicStage(EBGCinematicStage::None);
		}
		break;

	case EBGCinematicStage::None:
		//	연출 종료 — 주인공 참조를 놓아준다. 남겨 두면 죽은 캐릭터를 계속 붙들고 있게 된다.
		CinematicInstigator = nullptr;
		CinematicTarget     = nullptr;

		//	주의: 참조를 놓은 **뒤에** 부른다. 사망 처리 안에서 승패가 나면 판이 끝나는데,
		// 그 전에 연출 상태가 완전히 정리돼 있어야 한다.
		// 또한 IsCinematicPlaying() 이 이미 false 라 여기서 처리하는 사망이
		// 다시 대기열로 들어가는 일이 없다 — 순서가 뒤바뀌면 영영 처리되지 않는다.
		FlushPendingDeaths();
		break;
	}
}

void ABGGameState::OnRep_CinematicStage()
{
	//	같은 번들의 CinematicInstigator/Target 은 RepNotify 전에 모두 적용된다 — 여기서 읽어도 안전하다.
	HandleCinematicStage();
}

void ABGGameState::HandleCinematicStage()
{
	if (BroadcastedCinematicStage == CinematicStage)
	{
		return;   // 복제 재전송으로 같은 통지가 두 번 나가는 것을 막는다.
	}
	BroadcastedCinematicStage = CinematicStage;

	ABGCharacter* Subject = GetCinematicSubject();

	UE_LOG(LogBGCard, Log, TEXT("[Cinematic] 단계 %d — 주인공 %s"),
		static_cast<int32>(CinematicStage),
		Subject ? *Subject->GetName() : TEXT("없음"));

	//	── 카메라 연동 ──
	//	연출 단계가 곧 "누구의 B 카메라를 비출 것인가" 다. 이 두 줄만 지우면
	//	상태 관리는 그대로 두고 카메라만 직접 제어할 수 있다.
	if (HasAuthority())
	{
		//	단계 사이 이동 속도. None 단계면 Subject 가 nullptr → 각자 A 카메라로 복귀하며,
		//	그때는 이 값이 아니라 CinematicExitBlendSeconds(뷰타깃 전환)가 체감을 결정한다.
		SetCameraShot(Subject, CinematicShotBlendSeconds);
	}

	OnCinematicStageChanged.Broadcast(CinematicStage, Subject);
}

void ABGGameState::SetCameraShot(ABGCharacter* Target, float BlendSeconds, EBGShotBlend Blend)
{
	if (!HasAuthority())
	{
		return;
	}

	//	같은 대상으로 다시 걸면 무시한다 — 안 그러면 시작 시각이 갱신되어
	//	이미 도착한 카메라가 처음부터 다시 날아온다.
	if (CameraShot.Target == Target)
	{
		return;
	}

	CameraShot.Target       = Target;
	CameraShot.BlendSeconds = BlendSeconds;
	CameraShot.Blend        = Blend;

	//	서버 시간을 기준으로 삼아야 늦게 온 클라가 진행도를 계산할 수 있다.
	//	각자 자기 시계로 0 부터 세면 같은 순간에 서로 다른 그림을 본다.
	CameraShot.StartServerTime = GetServerWorldTimeSeconds();

	//	서버는 자기 OnRep 을 받지 못한다 — 여기서 직접 부르지 않으면 리슨서버 호스트만 연출이 안 뜬다.
	HandleCameraShot();

	UE_LOG(LogBGCard, Log, TEXT("[Camera] 샷 전환 — %s (%.2fs, %s)"),
		Target ? *Target->GetName() : TEXT("연출 종료"),
		BlendSeconds,
		Blend == EBGShotBlend::Orbit ? TEXT("선회") : TEXT("직선"));
}

void ABGGameState::OnRep_CameraShot()
{
	HandleCameraShot();
}

void ABGGameState::HandleCameraShot()
{
	//	여기서 뷰타깃을 직접 건드리지 않는다 — 화면은 PlayerController 의 몫이고,
	//	GameState 는 "무엇을 비출지" 만 안다. (UI/View 를 가른 것과 같은 이유)
	OnCameraShotChanged.Broadcast(CameraShot);
}

void ABGGameState::Multicast_CardResolved_Implementation(const FBGCardResolution& Resolution)
{
	//	리슨서버에서는 호스트에서도 이 본문이 실행된다 — 호스트 UI 도 같은 경로로 갱신된다.
	//	여기서 규칙을 처리하지 않는다. 판정과 효과 적용은 이미 서버에서 끝났고 이건 통지뿐이다.
	OnCardResolved.Broadcast(Resolution);
}

FGameplayTag ABGGameState::GameOverPhaseTag()
{
	return BG::Phase::GameOver.GetTag();
}

void ABGGameState::BeginPlay()
{
	Super::BeginPlay();

	//	덱은 서버만 만든다. 클라의 Deck 배열은 영원히 비어 있는 것이 정상
	if (HasAuthority())
	{
		InitializeDeck();
		SetPhaseTag(BG::Phase::Lobby.GetTag());
	}
}

void ABGGameState::SetPhaseTag(FGameplayTag InPhase)
{
	//	페이즈 전환은 반드시 이 함수 하나를 거친다.
	//	예전에는 권한 검사가 없는 SetPhase 가 따로 있어 턴 진행 전체가 검사를 우회했다.
	if (!HasAuthority() || PhaseTag == InPhase)
	{
		return;
	}

	PhaseTag = InPhase;
	UE_LOG(LogBGCard, Log, TEXT("[Turn] 좌석 %d — %s"), CurrentSeat, *InPhase.ToString());
	BroadcastTurnState();
}

void ABGGameState::SetCurrentSeat(int32 Seat)
{
	if (!HasAuthority() || CurrentSeat == Seat)
	{
		return;
	}

	CurrentSeat = Seat;
	BroadcastTurnState();
}

void ABGGameState::SetMatchRunning(bool bRunning)
{
	if (!HasAuthority() || bMatchRunning == bRunning)
	{
		return;
	}

	bMatchRunning = bRunning;
	BroadcastTurnState();
}

void ABGGameState::OnRep_TurnState()
{
	BroadcastTurnState();
}

void ABGGameState::BroadcastTurnState()
{
	OnTurnStateChanged.Broadcast();
}

void ABGGameState::InitializeDeck(int32 Seed)
{
	if (!HasAuthority())
	{
		return;
	}
	
	//	시드 보존
	if (bInitialized)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[InitializeDeck] 이미 초기화됨 — 무시. 재시작하려면 bInitialized 를 먼저 되돌릴 것."));
		return;
	}

	const UGameInstance* GI = GetGameInstance();
	UBGDataSubsystem* Subsystem = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogBGCard, Error, TEXT("[InitializeDeck] BGDataSubsystem 없음 — 덱을 만들지 못했습니다."));
		return;
	}

	//	Seed 0 은 "자동" 을 뜻한다. 실제로 쓴 값을 반드시 로그에 남겨야 같은 판을 재현할 수 있다.
	int32 ActualSeed = Seed;
	if (ActualSeed == 0)
	{
		ActualSeed = FMath::Rand();
		if (ActualSeed == 0)
		{
			ActualSeed = 1;   //	0 은 "자동" 의미로 예약되어 있으므로 시드로 쓰지 않는다.
		}
	}
	RandomStream.Initialize(ActualSeed);

	//	판정 스트림도 같은 매치 시드에서 파생시킨다(재현 가능). 수열은 셔플과 분리된다.
	if (Dice)
	{
		Dice->InitializeStream(ActualSeed);
	}

	Subsystem->BuildDeck(Deck);
	Discard.Reset();
	DiscardTop = FBGCardInstance();
	ShuffleDeck();
	RefreshPublicCounts();

	bInitialized = true;

	UE_LOG(LogBGCard, Warning, TEXT("[InitializeDeck] 덱 생성 완료. %d 장, 사용 시드 : %d"), Deck.Num(), ActualSeed);
}

bool ABGGameState::DrawFromDeck(FBGCardInstance& OutCard)
{
	if (!HasAuthority())
	{
		return false;
	}
	
	if (Deck.IsEmpty())
	{
		//	덱 소진 → 버린 패를 섞어 되돌린다. DiscardTop 은 공개 상태로 남으므로 섞이지 않는다.
		ReshuffleDeck();

		if (Deck.IsEmpty())
		{
			//	되돌릴 카드도 없다. 정상적인 판에서는 나오지 않는다.
			UE_LOG(LogBGCard, Warning, TEXT("[DrawFromDeck] 덱과 버린 패가 모두 비었습니다."));
			return false;
		}
	}

	//	RemoveAtSwap 을 쓰면 마지막 카드가 0 번으로 끌려와 셔플한 순서와 뽑는 순서가 달라진다.
	OutCard = Deck[0];
	Deck.RemoveAt(0);
	RefreshPublicCounts();
	return true;
}

void ABGGameState::PushToDiscard(const FBGCardInstance& Card)
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (DiscardTop.IsValid())
	{
		Discard.Add(DiscardTop);
	}

	//	맨 위의 버림카드를 새로 들어온 카드로 변경
	DiscardTop = Card;
	RefreshPublicCounts();
}

void ABGGameState::RefreshPublicCounts()
{
	DeckCount = Deck.Num();
	//	DiscardTop 은 Discard 배열 바깥에 따로 있으므로 더해 준다.
	DiscardCount = Discard.Num() + (DiscardTop.IsValid() ? 1 : 0);
}

bool ABGGameState::PerformDrawCheck(int32 Seat, float SuccessChance)
{
	//	TODO : 나중에 구현
	return false;
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  좌석 조회
//
//  "살아 있다" 의 정의를 여기 한곳에만 둔다. 턴 진행과 거리 계산이 서로 다른 기준을 쓰면
//  UI 는 살아 있다고 보는데 턴은 건너뛰는 식의 추적하기 어려운 버그가 난다.
// ══════════════════════════════════════════════════════════════════════════════════════

ABGPlayerState* ABGGameState::GetPlayerStateBySeat(int32 Seat) const
{
	if (Seat == INDEX_NONE)
	{
		return nullptr;
	}

	for (APlayerState* Base : PlayerArray)
	{
		ABGPlayerState* PS = Cast<ABGPlayerState>(Base);
		if (PS && PS->GetSeatIndex() == Seat)
		{
			return PS;
		}
	}
	return nullptr;
}

int32 ABGGameState::GetSeatHealth(int32 Seat) const
{
	const ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
	const UAbilitySystemComponent* ASC =
		PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn()) : nullptr;

	return ASC ? FMath::RoundToInt(ASC->GetNumericAttribute(UBGAttributeSet::GetHealthAttribute())) : 0;
}

bool ABGGameState::IsSeatAlive(int32 Seat) const
{
	const ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
	if (!PS)
	{
		return false;
	}

	//	사망은 PlayerState 의 복제 변수가 정본이다 — 서버·클라가 같은 답을 내야 한다.
	//	(ASC 의 State.Dead 루즈 태그는 복제되지 않아 클라에서 항상 false 다)
	if (PS->IsDead())
	{
		return false;
	}

	const UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn());
	if (!ASC)
	{
		//	스폰 전이거나 이탈로 폰이 파괴된 경우다. 둘 다 살아 있는 것으로 본다 —
		//	이탈을 사망으로 치면 자리를 비운 순간 그 진영이 전멸한 셈이 된다.
		return true;
	}

	return ASC->GetNumericAttribute(UBGAttributeSet::GetHealthAttribute()) > 0.f;
}

TArray<int32> ABGGameState::GetAliveSeatsInTableOrder() const
{
	TArray<int32> Seats;

	for (APlayerState* Base : PlayerArray)
	{
		const ABGPlayerState* PS = Cast<ABGPlayerState>(Base);
		if (PS && PS->GetSeatIndex() != INDEX_NONE && IsSeatAlive(PS->GetSeatIndex()))
		{
			Seats.Add(PS->GetSeatIndex());
		}
	}

	Seats.Sort();
	return Seats;
}

FGameplayTag ABGGameState::GetSeatRole(int32 Seat) const
{
	const ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
	if (!PS)
	{
		return FGameplayTag();
	}

	if (const ABGCharacter* Char = Cast<ABGCharacter>(PS->GetPawn()))
	{
		return Char->GetRoleTag();
	}

	//	폰이 없다. 무효 태그를 돌려주면 보안관이 튕긴 순간 무법자 승리가 나므로,
	//	배정 때 남긴 사본으로 답한다.
	return PS->GetCachedRole();
}

int32 ABGGameState::GetHandLimitForSeat(int32 Seat) const
{
	//	뱅 규칙: 손패 한도는 "현재" 생명력이다(최대치가 아니다).
	const int32 Health = GetSeatHealth(Seat);

	//	ASC 를 못 찾으면 GetSeatHealth 가 0 을 준다(폰 미스폰 / DefaultPawnClass 가 ABGCharacter 가 아님).
	//	그대로 한도 0 이 되면 손패를 전부 버려야만 턴이 넘어가고 그 사이 카드도 못 내는 잠금이 된다.
	//	설정 실수로 판이 멈추는 것보다 한도를 잠시 풀어두는 쪽이 낫다.
	if (Health <= 0)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[Turn] 좌석 %d 의 Health 를 읽지 못했습니다(ASC 없음?) — 손패 한도를 적용하지 않습니다."), Seat);
		return TNumericLimits<int32>::Max();
	}

	return Health;
}

int32 ABGGameState::GetCardUseLimitForSeat(int32 Seat) const
{
	const ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
	const UAbilitySystemComponent* ASC =
		PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn()) : nullptr;

	if (!ASC)
	{
		//	손패 한도와 달리 여기서는 "무제한" 으로 풀지 않는다.
		//	턴당 사용 제한은 이 게임의 핵심 행동 경제라(GDD §5.2) 설정 실패로 무력화되면 안 된다.
		return DefaultCardUseLimit;
	}

	const int32 Limit = FMath::RoundToInt(
		ASC->GetNumericAttribute(UBGAttributeSet::GetCardUseLimitAttribute()));
	return FMath::Max(0, Limit);
}

bool ABGGameState::HasCardUsesLeft(int32 Seat) const
{
	const ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
	if (!PS)
	{
		return false;
	}
	return PS->GetCardsUsedThisTurn() < GetCardUseLimitForSeat(Seat);
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  턴 진행
// ══════════════════════════════════════════════════════════════════════════════════════

void ABGGameState::StartMatch()
{
	if (!HasAuthority() || bMatchRunning)
	{
		return;
	}

	const TArray<int32> Alive = GetAliveSeatsInTableOrder();
	if (Alive.IsEmpty())
	{
		UE_LOG(LogBGCard, Error, TEXT("[Turn] StartMatch: 좌석이 배정된 플레이어가 없습니다."));
		return;
	}

	SetMatchRunning(true);

	//	초기 손패. InitialHandSize 가 0 이하면 좌석의 Health 만큼 준다(뱅 원작).
	for (const int32 Seat : Alive)
	{
		ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
		if (!PS)
		{
			continue;
		}

		const int32 Count = (InitialHandSize > 0) ? InitialHandSize : FMath::Max(1, GetSeatHealth(Seat));
		for (int32 i = 0; i < Count; ++i)
		{
			FBGCardInstance Card;
			if (!DrawFromDeck(Card))
			{
				break;
			}
			PS->AddCardToHand(Card);
		}

		UE_LOG(LogBGCard, Log, TEXT("[Turn] 초기 손패 — 좌석 %d : %d 장"), Seat, PS->GetHandCount());
	}

	//	뱅 규칙: 보안관이 선이다. 좌석과 역할은 따로 배정되므로 좌석 번호로 찾을 수 없다.
	int32 FirstSeat = Alive[0];
	for (const int32 Seat : Alive)
	{
		if (GetSeatRole(Seat) == BG::Role::Sheriff.GetTag())
		{
			FirstSeat = Seat;
			break;
		}
	}
	if (FirstSeat == Alive[0] && GetSeatRole(Alive[0]) != BG::Role::Sheriff.GetTag())
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Turn] 보안관 좌석을 찾지 못해 좌석 %d 로 시작합니다."), FirstSeat);
	}

	BeginTurn(FirstSeat);
}

void ABGGameState::BeginTurn(int32 Seat)
{
	if (!HasAuthority())
	{
		return;
	}

	SetCurrentSeat(Seat);
	SetPhaseTag(BG::Phase::TurnStart.GetTag());

	//	턴당 카드 사용 횟수를 새로 연다(GDD §5.2).
	if (ABGPlayerState* PS = GetPlayerStateBySeat(Seat))
	{
		PS->ResetCardsUsedThisTurn();
	}

	//	TODO: 감옥/다이너마이트 뽑기 판정이 여기 들어간다(PerformDrawCheck).
	//	      지금은 통과 페이즈다.

	EnterDrawPhase();

	//	밧줄은 드로우까지 끝난 뒤 붙인다 — 자동 처리에 제한 시간을 물릴 이유가 없다.
	//	자리를 비운 사람이면 짧은 밧줄이 붙어 곧바로 다음 좌석으로 넘어간다.
	if (Rope)
	{
		Rope->BeginRope(CurrentSeat);
	}
}

void ABGGameState::EnterDrawPhase()
{
	SetPhaseTag(BG::Phase::Draw.GetTag());

	ABGPlayerState* PS = GetPlayerStateBySeat(CurrentSeat);
	if (!PS)
	{
		EnterPlayPhase();
		return;
	}

	//	뽑을 장수는 어트리뷰트 DrawCount 에서 읽는다(Kit Carlson 등 캐릭터가 바꿀 수 있다).
	int32 DrawNum = DefaultDrawCount;
	if (const UAbilitySystemComponent* ASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn()))
	{
		const int32 Attr = FMath::RoundToInt(ASC->GetNumericAttribute(UBGAttributeSet::GetDrawCountAttribute()));
		if (Attr > 0)
		{
			DrawNum = Attr;
		}
	}

	int32 Drawn = 0;
	for (int32 i = 0; i < DrawNum; ++i)
	{
		FBGCardInstance Card;
		if (!DrawFromDeck(Card))
		{
			UE_LOG(LogBGCard, Warning, TEXT("[Turn] 덱 고갈 — 좌석 %d 은 %d 장만 뽑음"), CurrentSeat, Drawn);
			break;
		}
		PS->AddCardToHand(Card);
		++Drawn;
	}

	UE_LOG(LogBGCard, Log, TEXT("[Turn] 좌석 %d 뽑기 %d 장 (손패 %d, 덱 %d)"),
		CurrentSeat, Drawn, PS->GetHandCount(), DeckCount);

	EnterPlayPhase();
}

void ABGGameState::EnterPlayPhase()
{
	SetPhaseTag(BG::Phase::Play.GetTag());
}

bool ABGGameState::CanSeatPlayCards(int32 Seat) const
{
	return bMatchRunning
		&& !IsCinematicPlaying()   // 연출 중에는 전원 조작 불가
		&& Seat != INDEX_NONE
		&& Seat == CurrentSeat
		&& PhaseTag == BG::Phase::Play.GetTag();
}

bool ABGGameState::CanSeatDiscard(int32 Seat) const
{
	return bMatchRunning
		&& !IsCinematicPlaying()   // 연출 중에는 전원 조작 불가
		&& Seat != INDEX_NONE
		&& Seat == CurrentSeat
		&& PhaseTag == BG::Phase::Discard.GetTag();
}

void ABGGameState::RequestEndTurn(int32 RequestingSeat)
{
	if (!HasAuthority() || !bMatchRunning)
	{
		return;
	}

	//	연출 중에는 턴을 넘기지 않는다. 이 함수는 CanSeatPlayCards 를 거치지 않고
	//	직접 검사하므로 거기 넣은 연출 차단이 여기에는 적용되지 않는다 — 따로 막아야 한다.
	if (IsCinematicPlaying())
	{
		UE_LOG(LogBGCard, Log, TEXT("[Turn] EndTurn 보류 — 연출 재생 중"));
		return;
	}

	//	자기 턴이 아니면 무시. 클라가 마음대로 턴을 넘기지 못하게 한다.
	if (RequestingSeat != CurrentSeat || PhaseTag != BG::Phase::Play.GetTag())
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[Turn] EndTurn 거부 — 요청 좌석 %d, 현재 좌석 %d, 페이즈 %s"),
			RequestingSeat, CurrentSeat, *PhaseTag.ToString());
		return;
	}

	//	스스로 턴을 끝냈다 = 자리에 있다. 밧줄 기록을 지운다.
	if (ABGPlayerState* ActingPS = GetPlayerStateBySeat(CurrentSeat))
	{
		ActingPS->ResetRopeStrikes();
	}

	const ABGPlayerState* PS = GetPlayerStateBySeat(CurrentSeat);
	const int32 HandNum  = PS ? PS->GetHandCount() : 0;
	const int32 Limit    = GetHandLimitForSeat(CurrentSeat);

	if (HandNum > Limit)
	{
		//	한도를 넘었다 — 버릴 때까지 턴을 넘기지 않는다.
		SetPhaseTag(BG::Phase::Discard.GetTag());
		UE_LOG(LogBGCard, Log, TEXT("[Turn] 좌석 %d 손패 초과 (%d > 한도 %d) — 버리기 대기"),
			CurrentSeat, HandNum, Limit);
		return;
	}

	AdvanceToNextSeat();
}

void ABGGameState::ForceEndTurn(int32 Seat, const FString& Reason)
{
	if (!HasAuthority() || !bMatchRunning)
	{
		return;
	}

	//	이미 다른 사유로 턴이 넘어간 뒤 늦게 도착한 호출이다. 남의 턴을 뺏으면 안 된다.
	if (Seat != CurrentSeat)
	{
		UE_LOG(LogBGCard, Log, TEXT("[Turn] 강제 종료 무시 — 요청 좌석 %d, 현재 좌석 %d"),
			Seat, CurrentSeat);
		return;
	}

	//	손패 초과분을 앞에서부터 버린다. Discard 페이즈로 보내면 고를 사람이 없어 판이 멈춘다.
	if (ABGPlayerState* PS = GetPlayerStateBySeat(Seat))
	{
		const int32 Limit = GetHandLimitForSeat(Seat);
		int32 Dumped = 0;

		while (PS->GetHandCount() > Limit)
		{
			const TArray<FBGCardInstance>& Hand = PS->GetHand();
			if (Hand.IsEmpty())
			{
				break;   //	한도를 못 읽는 상황(ASC 없음)에서 무한 루프를 막는다.
			}

			FBGCardInstance Removed;
			if (!PS->RemoveCardFromHand(Hand[0].InstanceId, Removed))
			{
				break;
			}
			PushToDiscard(Removed);
			++Dumped;
		}

		if (Dumped > 0)
		{
			UE_LOG(LogBGCard, Log, TEXT("[Turn] 좌석 %d 손패 초과분 %d 장 자동 버림"), Seat, Dumped);
		}
	}

	UE_LOG(LogBGCard, Log, TEXT("[Turn] 좌석 %d 턴 강제 종료 — %s"), Seat, *Reason);
	AdvanceToNextSeat();
}

void ABGGameState::KillSeat(int32 Seat, const FString& Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
	if (!PS || PS->IsDead())
	{
		return;
	}

	UE_LOG(LogBGCard, Warning, TEXT("[Death] 좌석 %d 강제 사망 — %s"), Seat, *Reason);

	//	폰을 물고 있으면 평소 경로 그대로 — 피해 GE 가 Health 를 0 으로 만들고
	//	AttributeSet 이 사망을 통지한다.
	//
	//	주의: 두고 간 폰의 ASC 를 쓰면 안 된다. 그 폰은 PlayerState 가 없어
	//	ABGCharacter 의 사망 통지가 룰 계층까지 오지 못하고 판이 그대로 멈춘다.
	if (UAbilitySystemComponent* ASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn()))
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec =
			ASC->MakeOutgoingSpec(UGE_BGDamage::StaticClass(), 1.f, Context);

		if (Spec.IsValid())
		{
			//	남은 체력이 얼마든 확실히 넘기려고 크게 준다(피해는 음수로 넣는다).
			const float Lethal = static_cast<float>(FMath::Max(99, GetSeatHealth(Seat)));
			Spec.Data->SetSetByCallerMagnitude(BG::SetByCaller::Damage.GetTag(), -Lethal);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			return;
		}
	}

	//	이탈자다. 두고 간 폰은 PlayerState 가 없어 사망 통지가 오지 않으므로 직접 확정한다.
	UE_LOG(LogBGCard, Log, TEXT("[Death] 좌석 %d 컨트롤러 없음 — 사망을 직접 통지"), Seat);
	PS->SetDead(true);
	HandleSeatDeath(Seat, INDEX_NONE);
}

void ABGGameState::NotifyCardDiscarded(int32 Seat)
{
	if (!HasAuthority() || !CanSeatDiscard(Seat))
	{
		return;
	}

	const ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
	const int32 HandNum = PS ? PS->GetHandCount() : 0;
	const int32 Limit   = GetHandLimitForSeat(Seat);

	if (HandNum <= Limit)
	{
		AdvanceToNextSeat();
	}
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  사망 / 승패
// ══════════════════════════════════════════════════════════════════════════════════════

void ABGGameState::HandleSeatDeath(int32 DeadSeat, int32 KillerSeat)
{
	if (!HasAuthority())
	{
		return;
	}

	//	연출 중이면 규칙 처리를 미룬다.
	//	여기서 바로 처리하면 화면에서는 아직 공격 연출이 도는 중인데 카드가 버려지고
	//	승패까지 나 버린다 — 연출이 결과를 "보여주기 전에" 결과가 확정되는 셈이다.
	if (IsCinematicPlaying())
	{
		//	같은 좌석이 두 번 들어오지 않는다 — ABGCharacter::NotifyHealthDepleted 의
		//	bDead 플래그가 사망 통지 자체를 1회로 막는다. 그래도 방어적으로 확인한다.
		const bool bAlreadyQueued = PendingDeaths.ContainsByPredicate(
			[DeadSeat](const FBGPendingDeath& D) { return D.DeadSeat == DeadSeat; });

		if (!bAlreadyQueued)
		{
			PendingDeaths.Add({ DeadSeat, KillerSeat });
			UE_LOG(LogBGCard, Log,
				TEXT("[Death] 좌석 %d 사망 처리를 연출 후로 미룸 (대기 %d 건)"),
				DeadSeat, PendingDeaths.Num());
		}
		return;
	}

	ProcessSeatDeath(DeadSeat, KillerSeat);
}

void ABGGameState::FlushPendingDeaths()
{
	if (!HasAuthority() || PendingDeaths.IsEmpty())
	{
		return;
	}

	//	처리 도중 새 사망이 들어올 수 있다(보상 드로우가 다이너마이트를 터뜨리는 등).
	//	원본을 비우고 사본을 돌려야 그 새 사망이 이번 순회에 섞이지 않는다.
	TArray<FBGPendingDeath> Deaths = MoveTemp(PendingDeaths);
	PendingDeaths.Reset();

	UE_LOG(LogBGCard, Log, TEXT("[Death] 미뤄 둔 사망 %d 건 처리 시작"), Deaths.Num());

	//	TODO: 사망 연출은 이 자리에 들어온다. 지금은 규칙 처리로 바로 넘어가지만,
	//	   "사망자 확인 → 사망 연출(여럿이면 순차) → 규칙 처리" 순서가 되면
	//	   이 반복문이 연출을 하나 재생하고 끝나기를 기다린 뒤 다음으로 넘어가는 형태가 된다.
	//	   그때 ProcessSeatDeath 호출 시점은 각 사망 연출이 끝난 직후가 된다.
	for (const FBGPendingDeath& Death : Deaths)
	{
		ProcessSeatDeath(Death.DeadSeat, Death.KillerSeat);
	}
}

void ABGGameState::ProcessSeatDeath(int32 DeadSeat, int32 KillerSeat)
{
	if (!HasAuthority())
	{
		return;
	}

	const FGameplayTag DeadRole   = GetSeatRole(DeadSeat);
	const FGameplayTag KillerRole = GetSeatRole(KillerSeat);

	//	죽은 자리는 더 기다리지 않는다 — 예약이 남으면 죽은 좌석으로 재접속이 허용된다.
	if (UBGSeatComponent* SeatComp = UBGSeatComponent::Find(GetWorld()))
	{
		SeatComp->ClearReservation(DeadSeat);
	}

	//	이탈자가 두고 간 폰은 주인이 없어 아무도 치우지 않는다. 죽었으니 여기서 내보낸다 —
	//	안 그러면 시체가 자리에 계속 서 있는다.
	if (ABGPlayerState* DeadPS = GetPlayerStateBySeat(DeadSeat))
	{
		if (APawn* Abandoned = DeadPS->GetAbandonedPawn())
		{
			DeadPS->ClearAbandonedPawn();
			Abandoned->Destroy();
		}
	}

	// ── 1. 사망자의 역할을 공개하고 손패·장비를 전부 버린 패로 ──
	if (ABGPlayerState* DeadPS = GetPlayerStateBySeat(DeadSeat))
	{
		//	뱅 규칙: 죽으면 역할이 공개된다. 이 시점부터 전원이 볼 수 있다.
		DeadPS->SetPublicRole(DeadRole);

		//	걸려 있던 "다음 1회" 상태는 사라진다(발동될 다음 카드가 없다).
		DeadPS->ClearPendingStatuses();

		TArray<FBGCardInstance> Dropped;
		DeadPS->TakeAllCards(Dropped);
		for (const FBGCardInstance& Card : Dropped)
		{
			PushToDiscard(Card);
		}

		UE_LOG(LogBGCard, Log, TEXT("[Death] 좌석 %d(%s) 카드 %d 장 버림"),
			DeadSeat, *DeadRole.ToString(), Dropped.Num());
	}

	// ── 2. 보상 / 벌칙 (뱅 원작) ──
	if (KillerSeat != INDEX_NONE && KillerSeat != DeadSeat && IsSeatAlive(KillerSeat))
	{
		if (DeadRole == BG::Role::Outlaw.GetTag())
		{
			// 무법자를 잡으면 현상금 3장.
			ABGPlayerState* KillerPS = GetPlayerStateBySeat(KillerSeat);
			int32 Drawn = 0;
			for (int32 i = 0; KillerPS && i < OutlawKillReward; ++i)
			{
				FBGCardInstance Card;
				if (!DrawFromDeck(Card))
				{
					break;
				}
				KillerPS->AddCardToHand(Card);
				++Drawn;
			}
			UE_LOG(LogBGCard, Log, TEXT("[Death] 무법자 사살 보상 — 좌석 %d 가 %d 장 획득"), KillerSeat, Drawn);
		}
		else if (DeadRole == BG::Role::Deputy.GetTag() && KillerRole == BG::Role::Sheriff.GetTag())
		{
			// 보안관이 부관을 쐈다 — 손패와 장비를 전부 잃는다.
			if (ABGPlayerState* SheriffPS = GetPlayerStateBySeat(KillerSeat))
			{
				TArray<FBGCardInstance> Lost;
				SheriffPS->TakeAllCards(Lost);
				for (const FBGCardInstance& Card : Lost)
				{
					PushToDiscard(Card);
				}
				UE_LOG(LogBGCard, Log, TEXT("[Death] 보안관이 부관 사살 — 좌석 %d 카드 %d 장 몰수"),
					KillerSeat, Lost.Num());
			}
		}
	}

	// ── 3. 승패 판정 ──
	if (CheckWinCondition())
	{
		return;   // 판이 끝났으면 턴을 넘기지 않는다.
	}

	// ── 4. 죽은 사람이 현재 턴이었다면 턴을 넘긴다 ──
	if (DeadSeat == CurrentSeat && bMatchRunning)
	{
		AdvanceToNextSeat();
	}
}

bool ABGGameState::CheckWinCondition()
{
	if (!bMatchRunning)
	{
		return false;
	}

	const TArray<int32> Alive = GetAliveSeatsInTableOrder();

	int32 NumSheriff = 0, NumDeputy = 0, NumOutlaw = 0, NumRenegade = 0;
	for (const int32 Seat : Alive)
	{
		//	'Role' 은 AActor 의 멤버명이라 섀도잉을 피한다 (C4458 은 에러로 승격됨).
		const FGameplayTag SeatRole = GetSeatRole(Seat);
		if      (SeatRole == BG::Role::Sheriff.GetTag())  { ++NumSheriff; }
		else if (SeatRole == BG::Role::Deputy.GetTag())   { ++NumDeputy; }
		else if (SeatRole == BG::Role::Outlaw.GetTag())   { ++NumOutlaw; }
		else if (SeatRole == BG::Role::Renegade.GetTag()) { ++NumRenegade; }
	}

	FGameplayTag Winner;

	if (NumSheriff == 0)
	{
		// 보안관이 죽었다. 배신자가 마지막 1인으로 남았으면 배신자 승, 아니면 무법자 승.
		Winner = (Alive.Num() == 1 && NumRenegade == 1)
			? BG::Role::Renegade.GetTag()
			: BG::Role::Outlaw.GetTag();
	}
	else if (NumOutlaw == 0 && NumRenegade == 0)
	{
		// 보안관 생존 + 적대 진영 전멸 → 보안관 진영 승.
		Winner = BG::Role::Sheriff.GetTag();
	}

	if (!Winner.IsValid())
	{
		return false;
	}

	WinningRoleTag = Winner;
	SetCurrentSeat(INDEX_NONE);
	SetMatchRunning(false);
	SetPhaseTag(BG::Phase::GameOver.GetTag());

	//	판이 끝났으니 타고 있던 밧줄도 끈다 — 안 끄면 종료 연출 중에 만료가 튀어나온다.
	if (Rope)
	{
		Rope->StopRope();
	}

	//	판이 끝나면 전원의 역할을 공개한다(누가 배신자였는지 보는 것이 마무리의 재미다).
	for (APlayerState* Base : PlayerArray)
	{
		if (ABGPlayerState* PS = Cast<ABGPlayerState>(Base))
		{
			PS->SetPublicRole(GetSeatRole(PS->GetSeatIndex()));
		}
	}

	//	승패 분쟁은 QA 에서 반드시 생긴다. 승자만 찍으면 재현이 불가능하므로 근거(좌석별 진영)를 남긴다.
	TArray<FString> AliveDesc;
	for (const int32 Seat : Alive)
	{
		AliveDesc.Add(FString::Printf(TEXT("Seat%d(%s)"), Seat, *GetSeatRole(Seat).ToString()));
	}
	UE_LOG(LogBGCard, Warning, TEXT("[Match] 판 종료 — 승리 진영 : %s / 생존 %d명 : %s"),
		*Winner.ToString(), Alive.Num(), *FString::Join(AliveDesc, TEXT(", ")));

	//	판정이 끝났으니 종료 연출을 연다. 승리 → 크레딧 → 로비 복귀는 전부 서버가 진행시킨다.
	SetMatchEndStage(EBGMatchEndStage::Victory);
	return true;
}

void ABGGameState::SetMatchEndStage(EBGMatchEndStage NewStage)
{
	if (!HasAuthority() || MatchEndStage == NewStage)
	{
		return;
	}

	MatchEndStage = NewStage;

	//	서버는 자기 OnRep 을 받지 못한다 — 여기서 직접 쏘지 않으면 리슨서버 호스트만 연출이 안 뜬다.
	HandleMatchEndStage();

	//	다음 단계 예약. 타이머는 서버에만 하나 돈다(클라가 각자 세면 늦게 접속한 사람이 어긋난다).
	GetWorldTimerManager().ClearTimer(MatchEndStageTimer);

	switch (NewStage)
	{
	case EBGMatchEndStage::Victory:
		if (VictorySequenceSeconds > 0.f)
		{
			GetWorldTimerManager().SetTimer(MatchEndStageTimer, this,
				&ABGGameState::StartEndingCredits, VictorySequenceSeconds, false);
		}
		else
		{
			StartEndingCredits();
		}
		break;

	case EBGMatchEndStage::Credits:
		if (!bReturnToLobbyAfterCredits)
		{
			UE_LOG(LogBGCard, Log, TEXT("[Match] 크레딧 — 로비 복귀 꺼짐(bReturnToLobbyAfterCredits=false)."));
			break;
		}
		if (EndingCreditsSeconds > 0.f)
		{
			GetWorldTimerManager().SetTimer(MatchEndStageTimer, this,
				&ABGGameState::ReturnToLobby, EndingCreditsSeconds, false);
		}
		else
		{
			ReturnToLobby();
		}
		break;

	default:
		break;
	}
}

void ABGGameState::StartEndingCredits()
{
	if (!HasAuthority() || MatchEndStage != EBGMatchEndStage::Victory)
	{
		return;   // 아직 승리 연출 중이 아니거나 이미 넘어갔다.
	}
	SetMatchEndStage(EBGMatchEndStage::Credits);
}

void ABGGameState::OnRep_MatchEndStage()
{
	//	같은 번들의 프로퍼티(WinningRoleTag)는 RepNotify 가 불리기 전에 모두 적용된다 —
	//	여기서 승리 진영을 읽어도 안전하다.
	HandleMatchEndStage();
}

void ABGGameState::HandleMatchEndStage()
{
	if (BroadcastedStage == MatchEndStage)
	{
		return;   // 복제 재전송으로 같은 델리게이트가 두 번 나가는 것을 막는다.
	}
	BroadcastedStage = MatchEndStage;

	switch (MatchEndStage)
	{
	case EBGMatchEndStage::Victory:
		UE_LOG(LogBGCard, Log, TEXT("[Match] 승리 연출 시작 — 승리 진영 %s"), *WinningRoleTag.ToString());
		OnMatchWon.Broadcast(WinningRoleTag);
		break;

	case EBGMatchEndStage::Credits:
		UE_LOG(LogBGCard, Log, TEXT("[Match] 엔딩 크레딧 시작"));
		OnEndingCredits.Broadcast();
		break;

	default:
		break;
	}
}

void ABGGameState::ReturnToLobby()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	//	제자리에서 상태를 하나씩 되돌리지 않는 이유:
	//	  로비와 게임이 같은 맵이라(BGGameInstance 의 "단일 맵" 주석 참고) 판을 끝내면
	//	  덱·손패·장비 GE·역할/직업 태그·좌석·사망 플래그·어트리뷰트가 전부 남는다.
	//	  하나라도 빠뜨리면 "두 번째 판에서만" 나오는 버그가 되고 추적이 매우 어렵다.
	//	  같은 맵을 다시 열면 월드가 통째로 새로 만들어져 지울 목록이라는 개념 자체가 사라진다.
	//	  클라이언트는 서버를 따라 함께 이동하므로 접속은 유지된다.

	//	주의: GameInstance 는 트래블을 넘어 살아남는다 — 여기만 직접 되돌려야 한다.
	// 빠뜨리면 새 로비가 난입 차단 상태로 열려 아무도 들어오지 못한다.
	if (UBGGameInstance* GI = GetGameInstance<UBGGameInstance>())
	{
		GI->SetAllowJoinInProgress(true);
	}

	UE_LOG(LogBGCard, Warning, TEXT("[Match] 로비 복귀 — 같은 맵 재시작(ServerTravel)"));

	//	"?Restart" 는 "맵 이름 없이 현재 맵을 다시 연다" 는 엔진 규약이다.
	//	AGameMode::RestartGame() 이 쓰는 것과 같은 경로 — 다만 이 프로젝트는 AGameModeBase
	//	상속이라 그 함수가 없어 직접 부른다. 맵 이름을 문자열로 짜 넣지 않으므로
	//	맵을 옮기거나 이름을 바꿔도 깨지지 않는다.
	World->ServerTravel(TEXT("?Restart"), /*bAbsolute=*/false);
}

void ABGGameState::AdvanceToNextSeat()
{
	//	넘어가는 동안에는 밧줄이 타지 않는다. 새 좌석의 BeginTurn 이 다시 붙인다.
	if (Rope)
	{
		Rope->StopRope();
	}

	const TArray<int32> Alive = GetAliveSeatsInTableOrder();
	if (Alive.IsEmpty())
	{
		//	전멸 — 승패 판정이 붙기 전까지는 여기서 멈춘다.
		UE_LOG(LogBGCard, Warning, TEXT("[Turn] 살아있는 좌석이 없습니다 — 진행 중단."));
		SetPhaseTag(BG::Phase::GameOver.GetTag());
		SetMatchRunning(false);
		return;
	}

	//	현재 좌석이 죽어서 목록에 없을 수 있다. 그때는 "다음으로 큰 좌석" 을 찾는다.
	int32 NextIndex = 0;
	const int32 Found = Alive.IndexOfByKey(CurrentSeat);
	if (Found != INDEX_NONE)
	{
		NextIndex = (Found + 1) % Alive.Num();
	}
	else
	{
		for (int32 i = 0; i < Alive.Num(); ++i)
		{
			if (Alive[i] > CurrentSeat)
			{
				NextIndex = i;
				break;
			}
		}
	}

	BeginTurn(Alive[NextIndex]);
}

void ABGGameState::GetDeck(TArray<FBGCardInstance>& OutDeck) const
{
	OutDeck = Deck;   //	대입이 기존 내용을 버린다 — 앞에 Empty() 를 부를 필요가 없다.
}

void ABGGameState::ShuffleDeck()
{
	for (int i = Deck.Num() - 1; i > 0; --i)
	{
		int j = RandomStream.RandRange(0, i);
		Deck.Swap(i, j);
	}
}

void ABGGameState::ReshuffleDeck()
{	//	버림덱의 맨 위 카드(DiscardTop)를 제외하고 셔플하여 기존 덱에 추가
	if (Discard.IsEmpty())
	{
		return;
	}

	for (int i = Discard.Num() - 1; i > 0; --i)
	{
		int j = RandomStream.RandRange(0, i);
		Discard.Swap(i, j);
	}

	UE_LOG(LogBGCard, Log, TEXT("[ReshuffleDeck] 버린 패 %d 장을 덱으로 되돌립니다."), Discard.Num());

	Deck.Append(Discard);
	Discard.Empty();
	RefreshPublicCounts();
}


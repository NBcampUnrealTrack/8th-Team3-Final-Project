// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/BGGameState.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/Effect/GE_BGHeal.h"
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
#include "AI/BGAIPlayerController.h"
#include "Core/BGGameMode.h"

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
	DOREPLIFETIME(ABGGameState, EnemyDeckCount);
	DOREPLIFETIME(ABGGameState, EnemyDiscardCount);
	DOREPLIFETIME(ABGGameState, EnemyDiscardTop);
	DOREPLIFETIME(ABGGameState, PhaseTag);
	DOREPLIFETIME(ABGGameState, CurrentSeat);
	DOREPLIFETIME(ABGGameState, WinningRoleTag);
	DOREPLIFETIME(ABGGameState, bMatchRunning);
	DOREPLIFETIME(ABGGameState, MatchEndStage);
	DOREPLIFETIME(ABGGameState, CameraShot);
	DOREPLIFETIME(ABGGameState, CinematicStage);
	DOREPLIFETIME(ABGGameState, CinematicInstigator);
	DOREPLIFETIME(ABGGameState, CinematicTarget);
	DOREPLIFETIME(ABGGameState, CinematicResolution);
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

	//	사망 연출은 두 단계 모두 사망자를 비춘다. TryBeginNextDeathCinematic 이
	//	CinematicInstigator 에 사망자를 넣어 둔다("이번 단계의 주인공" 이라는 뜻으로 쓴다).
	case EBGCinematicStage::DeathPrimary:
	case EBGCinematicStage::DeathSecondary: return CinematicInstigator;

	default:                           return nullptr;
	}
}

void ABGGameState::BeginCardCinematic(ABGCharacter* Caster, ABGCharacter* Victim,
	const FBGCardResolution& Resolution)
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
	CinematicResolution = Resolution;

	SetCinematicStage(EBGCinematicStage::Primary);
}

void ABGGameState::SetCinematicStage(EBGCinematicStage NewStage)
{
	if (!HasAuthority() || CinematicStage == NewStage)
	{
		return;
	}

	CinematicStage = NewStage;

	// ── 통지 전에 상태를 완성해 둔다 ──
	//	⚠️ 아래 HandleCinematicStage(통지)보다 **먼저** 해야 한다. 클라는 이 값들을 단계와
	//	   같은 번들로 받으므로 OnRep 시점에 이미 반영돼 있다 — 서버만 옛 값을 들고 통지하면
	//	   같은 순간에 서버와 클라의 애니메이션·UI 분기가 어긋난다.
	switch (NewStage)
	{
	case EBGCinematicStage::None:
		//	연출 종료 — 주인공과 판정 참조를 놓아준다.
		//	남겨 두면 죽은 캐릭터를 계속 붙들고 있게 된다.
		CinematicInstigator = nullptr;
		CinematicTarget     = nullptr;
		CinematicResolution = FBGCardResolution();
		ActiveDeath         = FBGPendingDeath();
		break;

	case EBGCinematicStage::DeathPrimary:
		//	이 순간부터 화면에서도 죽은 것으로 본다 — 여기까지는 평소 포즈로 서 있었다.
		RevealDeath(ActiveDeath.DeadSeat);
		break;

	case EBGCinematicStage::DeathSecondary:
		//	역할 공개 연출이 시작되기 **전에** 실제로 공개한다.
		//	공개가 통지보다 늦으면 연출이 도는 첫 순간에 UI 가 아직 빈 역할을 읽는다.
		RevealDeadRole(ActiveDeath.DeadSeat);
		break;

	default:
		break;
	}

	//	연출이 도는 동안 밧줄을 멈춘다. 안 그러면 연출 시간만큼 자기 턴을 깎아 먹는다.
	//	단계가 Primary → Secondary 로 넘어갈 때는 같은 이유라 잡은 상태가 그대로 유지된다.
	if (Rope)
	{
		if (IsCinematicPlaying())
		{
			Rope->PauseRope(BGRopePause::Cinematic);
		}
		else
		{
			Rope->ResumeRope(BGRopePause::Cinematic);
		}
	}

	//	서버는 자기 OnRep 을 받지 못한다 — 여기서 직접 부르지 않으면 리슨서버 호스트만 연출이 안 뜬다.
	HandleCinematicStage();

	GetWorldTimerManager().ClearTimer(CinematicStageTimer);

	//	다음 단계 예약.
	switch (NewStage)
	{
	case EBGCinematicStage::Primary:
		ScheduleCinematicStep(PrimaryCinematicSeconds,
			[this]() { SetCinematicStage(EBGCinematicStage::Secondary); });
		break;

	case EBGCinematicStage::Secondary:
		//	카드 연출의 끝. None 으로 바로 가지 않는다 — 밀린 사망이 있으면 이어서 재생한다.
		ScheduleCinematicStep(SecondaryCinematicSeconds,
			[this]() { EndCardCinematic(); });
		break;

	case EBGCinematicStage::DeathPrimary:
		ScheduleCinematicStep(DeathPrimarySeconds,
			[this]() { SetCinematicStage(EBGCinematicStage::DeathSecondary); });
		break;

	case EBGCinematicStage::DeathSecondary:
		//	역할 공개가 끝나야 결과가 나온다 — 보상·승패는 FinishDeathCinematic 안에 있다.
		ScheduleCinematicStep(DeathSecondarySeconds,
			[this]() { FinishDeathCinematic(); });
		break;

	case EBGCinematicStage::None:
		break;
	}
}

void ABGGameState::ScheduleCinematicStep(float DelaySeconds, TFunction<void()>&& Step)
{
	if (DelaySeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(CinematicStageTimer, MoveTemp(Step), DelaySeconds, /*bLoop=*/false);
		return;
	}

	//	길이가 0 이하면 기다리지 않고 즉시 넘어간다 — 연출을 끄고 규칙만 돌릴 때 쓴다.
	Step();
}

void ABGGameState::EndCardCinematic()
{
	//	이번 카드로 죽은 사람이 있으면 화면을 돌려주지 않고 곧장 사망 연출로 넘어간다.
	if (TryBeginNextDeathCinematic())
	{
		return;
	}

	SetCinematicStage(EBGCinematicStage::None);
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
	//	주사위가 화면에 떠 있는 동안 밧줄을 멈춘다. 판정 표시는 클라 위젯의 일이라 서버에
	//	"끝났다" 통지가 없다 — 같은 길이의 타이머로 놓는다. 연속 판정이면 타이머가 갱신된다.
	if (HasAuthority() && Rope && ResolutionRopePauseSeconds > 0.f)
	{
		Rope->PauseRope(BGRopePause::Resolution);
		GetWorldTimerManager().SetTimer(ResolutionPauseTimer,
			[this]() { if (Rope) { Rope->ResumeRope(BGRopePause::Resolution); } },
			ResolutionRopePauseSeconds, /*bLoop=*/false);
	}

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

	//	브로드캐스트는 다음 줄 SetPhaseTag 가 한다. 여기서 하면 PhaseTag 가 아직 이전 좌석
	//	값이라 봇이 드로우 전 낡은 손패로 판단해버린다.
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

void ABGGameState::InitializeDeck(int32 Seed, int32 EncounterIndex)
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

	//	PvP 는 기존 경로 그대로. 아래 진영별 분리는 PvE 전용이다.
	const bool bIsPvE = ABGGameMode::GetPlayMode(this) == EBGPlayMode::PvE;
	if (bIsPvE)
	{
		//	Dice 스트림과 같은 이유로 매치 시드에서 파생시킨다. 재현 가능하되 Player 셔플과는
		//	다른 수열을 쓴다. 상수는 Dice 것과 겹치지 않는 별개의 홀수 소수를 쓴다.
		const int32 EnemySeed = ActualSeed * 2246822519u + 3266489917u;
		EnemyRandomStream.Initialize(EnemySeed);

		Subsystem->BuildPvEDeck(Deck, EBGDeckSide::Player, EncounterIndex);
		Subsystem->BuildPvEDeck(EnemyDeck, EBGDeckSide::Enemy, EncounterIndex);
		EnemyDiscard.Reset();
		EnemyDiscardTop = FBGCardInstance();
		ShuffleDeck(EBGDeckSide::Enemy);
	}
	else
	{
		Subsystem->BuildDeck(Deck);
	}
	Discard.Reset();
	DiscardTop = FBGCardInstance();
	ShuffleDeck();
	RefreshPublicCounts();

	bInitialized = true;

	UE_LOG(LogBGCard, Warning, TEXT("[InitializeDeck] 덱 생성 완료. %d 장, 사용 시드 : %d"), Deck.Num(), ActualSeed);
}

bool ABGGameState::DrawFromDeck(FBGCardInstance& OutCard, EBGDeckSide Side)
{
	if (!HasAuthority())
	{
		return false;
	}

	TArray<FBGCardInstance>& TargetDeck = (Side == EBGDeckSide::Enemy) ? EnemyDeck : Deck;

	if (TargetDeck.IsEmpty())
	{
		//	덱 소진 → 버린 패를 섞어 되돌린다. DiscardTop 은 공개 상태로 남으므로 섞이지 않는다.
		ReshuffleDeck(Side);

		if (TargetDeck.IsEmpty())
		{
			//	되돌릴 카드도 없다. 정상적인 판에서는 나오지 않는다.
			UE_LOG(LogBGCard, Warning, TEXT("[DrawFromDeck] 덱과 버린 패가 모두 비었습니다 (Side=%d)."), (int32)Side);
			return false;
		}
	}

	//	RemoveAtSwap 을 쓰면 마지막 카드가 0 번으로 끌려와 셔플한 순서와 뽑는 순서가 달라진다.
	OutCard = TargetDeck[0];
	TargetDeck.RemoveAt(0);
	RefreshPublicCounts();
	return true;
}

void ABGGameState::PushToDiscard(const FBGCardInstance& Card)
{
	if (!HasAuthority())
	{
		return;
	}

	//	Card.Side 는 BuildPvEDeck 이 만들 때 이미 찍어뒀다. Seat 을 다시 조회할 필요가 없다.
	//	PvP 카드는 항상 Player 라 아래는 예전과 동일하게 동작한다.
	FBGCardInstance& TargetTop = (Card.Side == EBGDeckSide::Enemy) ? EnemyDiscardTop : DiscardTop;
	TArray<FBGCardInstance>& TargetDiscard = (Card.Side == EBGDeckSide::Enemy) ? EnemyDiscard : Discard;

	if (TargetTop.IsValid())
	{
		TargetDiscard.Add(TargetTop);
	}

	//	맨 위의 버림카드를 새로 들어온 카드로 변경
	TargetTop = Card;
	RefreshPublicCounts();
}

EBGDeckSide ABGGameState::GetDeckSideForSeat(int32 Seat) const
{
	const ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
	if (PS && Cast<ABGAIPlayerController>(PS->GetOwningController()))
	{
		return EBGDeckSide::Enemy;
	}
	return EBGDeckSide::Player;
}

void ABGGameState::RefreshPublicCounts()
{
	DeckCount = Deck.Num();
	//	DiscardTop 은 Discard 배열 바깥에 따로 있으므로 더해 준다.
	DiscardCount = Discard.Num() + (DiscardTop.IsValid() ? 1 : 0);

	EnemyDeckCount = EnemyDeck.Num();
	EnemyDiscardCount = EnemyDiscard.Num() + (EnemyDiscardTop.IsValid() ? 1 : 0);
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

int32 ABGGameState::GetHandLimitForSeat(int32 /*Seat*/) const
{
	//	[비활성] 예전에는 뱅 원작대로 현재 생명력이 한도였다(GetSeatHealth).
	//	  체력이 줄 때마다 손패를 버려야 했고, 그래서 ASC 를 못 읽으면 한도 0 으로 판이 멈추는
	//	  위험이 있어 폴백까지 두고 있었다. 상한을 드로우 시점에 막는 지금은 그 위험이 없다.
	//	  되살리려면 이 함수가 GetSeatHealth(Seat) 를 돌려주게 하면 된다(호출부는 그대로 둬도 된다).

	//	0 이하면 "상한 없음" 이다.
	return (MaxHandSize > 0) ? MaxHandSize : TNumericLimits<int32>::Max();
}

int32 ABGGameState::DrawCardsToHand(ABGPlayerState* PS, int32 Count)
{
	if (!HasAuthority() || !PS || Count <= 0)
	{
		return 0;
	}

	const int32 Limit = GetHandLimitForSeat(PS->GetSeatIndex());
	const EBGDeckSide Side = GetDeckSideForSeat(PS->GetSeatIndex());

	int32 Drawn = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		//	⚠️ 상한 검사가 DrawFromDeck 보다 먼저다. 순서가 반대면 덱에서 이미 빠진 카드를
		//	   손패에 넣지 못해 그대로 사라진다(덱에도 버린 패에도 없는 유령 카드가 된다).
		if (PS->GetHandCount() >= Limit)
		{
			UE_LOG(LogBGCard, Log,
				TEXT("[Draw] 좌석 %d 손패 상한 %d 도달 — %d 장 중 %d 장만 뽑음"),
				PS->GetSeatIndex(), Limit, Count, Drawn);
			break;
		}

		FBGCardInstance Card;
		if (!DrawFromDeck(Card, Side))
		{
			break;   // 덱과 버린 패가 모두 비었다
		}

		PS->AddCardToHand(Card);
		++Drawn;
	}

	return Drawn;
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
		DrawCardsToHand(PS, Count);

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

	//	치유사(TurnStartHeal) — 자기 턴이 열릴 때 회복한다.
	//	카드 효과가 아니라 턴 진행의 일부라 GA 가 아니라 여기 있다.
	ApplyTurnStartHeal(Seat);

	//	TODO: 감옥/다이너마이트 뽑기 판정이 여기 들어간다(PerformDrawCheck).
	//	      지금은 통과 페이즈다.

	EnterDrawPhase();

	//	밧줄은 드로우까지 끝난 뒤 붙인다 — 자동 처리에 제한 시간을 물릴 이유가 없다.
	//	자리를 비운 사람이면 짧은 밧줄이 붙어 곧바로 다음 좌석으로 넘어간다.
	//	AI 턴에는 밧줄을 안 붙인다.
	const ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
	const bool bIsAISeat = PS && Cast<ABGAIPlayerController>(PS->GetOwningController()) != nullptr;
	if (Rope && !bIsAISeat)
	{
		//	CurrentSeat 대신 Seat 사용. 재귀 중첩 시 CurrentSeat 은 이미 안쪽 호출이 바꾼
		//	값이라 엉뚱한 좌석에 로프가 걸린다.
		Rope->BeginRope(Seat);
	}
}

void ABGGameState::ApplyTurnStartHeal(int32 Seat)
{
	if (!HasAuthority())
	{
		return;
	}

	const ABGPlayerState* PS = GetPlayerStateBySeat(Seat);
	if (!PS || PS->IsDead())
	{
		return;
	}

	UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn());
	if (!ASC)
	{
		return;
	}

	const int32 HealAmount = FMath::RoundToInt(
		ASC->GetNumericAttribute(UBGAttributeSet::GetTurnStartHealAttribute()));
	if (HealAmount <= 0)
	{
		return;   // 기본값 0 — 치유사가 아니면 아무 일도 일어나지 않는다
	}

	//	이미 만피면 GE 를 굴리지 않는다. 적용해도 AttributeSet 이 클램프하지만,
	//	로그가 "회복 1" 로 남아 실제로는 안 오른 체력을 올랐다고 읽게 된다.
	if (ASC->GetNumericAttribute(UBGAttributeSet::GetHealthAttribute())
		>= ASC->GetNumericAttribute(UBGAttributeSet::GetMaxHealthAttribute()))
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec =
		ASC->MakeOutgoingSpec(UGE_BGHeal::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogBGCard, Warning, TEXT("[턴회복] GE_BGHeal Spec 생성 실패 — 좌석 %d"), Seat);
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(BG::SetByCaller::Heal.GetTag(), static_cast<float>(HealAmount));
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	UE_LOG(LogBGCard, Log, TEXT("[턴회복] 좌석 %d 이 턴 시작 회복 %d"), Seat, HealAmount);
}

int32 ABGGameState::GrantCards(ABGPlayerState* PS, FGameplayTag CardId, int32 Count)
{
	if (!HasAuthority() || !PS || !CardId.IsValid() || Count <= 0)
	{
		return 0;
	}

	//	Row 이름 = 태그 문자열이 이 프로젝트의 규약이다(Card-Data-Spec.md).
	const FName RowName = CardId.GetTagName();

	//	DT 에 없는 카드를 만들면 손패에 "이름 없는 카드" 가 생겨 사용도 안 되고 지워지지도 않는다.
	const UGameInstance* GI = GetGameInstance();
	const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	const FBGCardRow* Row = Data ? Data->GetCardRow(RowName) : nullptr;
	if (!Row)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[카드지급] DT_BGCard 에 Row '%s' 가 없어 지급하지 않습니다."), *RowName.ToString());
		return 0;
	}

	//	일회용인지는 **카드가 정한다.** 호출자는 몰라도 된다(헤더 주석 참조).
	const bool bEphemeral = Row->TraitTags.HasTag(BG::Card::Trait::Ephemeral.GetTag());

	//	일회용이 아닌 카드를 만들어 주면 **덱 총량이 늘어난다** — 소비될 때 버린 패로 가고,
	//	버린 패는 셔플되어 덱으로 돌아오기 때문이다. `BG.ExpectedDeckSize 64` 검증이 그만큼 어긋난다.
	//	막지는 않는다(PvE 에서 의도적으로 카드를 늘릴 수 있어야 한다). 대신 조용히 넘어가지 않는다 —
	//	덱 장수가 안 맞을 때 원인을 여기서 찾을 수 있어야 한다.
	//	덱에 있는 카드를 주고 싶은 것이라면 이 함수가 아니라 **덱에서 뽑아 옮기는 쪽**이 맞다.
	if (!bEphemeral)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[카드지급] '%s' 는 일회용이 아닙니다 — 만들어 주면 덱 총량이 %d장 늘어납니다. "
			     "의도한 것이 아니면 DT_BGCard 의 TraitTags 에 Card.Trait.Ephemeral 을 넣으세요."),
			*RowName.ToString(), Count);
	}

	//	손패 상한은 현재 체력이다 — 덱에서 뽑을 때와 같은 규칙을 쓴다.
	int32 Granted = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		if (PS->GetHandCount() >= GetHandLimitForSeat(PS->GetSeatIndex()))
		{
			break;
		}

		FBGCardInstance Card;
		Card.CardId     = RowName;
		Card.bEphemeral = bEphemeral;

		//	일회용만 음수 InstanceId 를 쓴다. -2 부터 내려간다 — -1(INDEX_NONE)은 RPC 검증에서
		//	무효 취급이라 그 카드를 내는 클라이언트가 연결 종료된다(헤더 주석 참조).
		//
		//	일반 카드를 이 경로로 주면 덱 InstanceId 와 겹칠 수 있으므로 음수를 그대로 쓴다.
		//	덱 총량 회계는 영향받지 않는다 — 소비되면 버린 패로 가고, 그건 덱에 원래 없던 한 장이
		//	늘어나는 것이라 **의도한 경우에만** 써야 한다(PvE 이벤트 등).
		Card.InstanceId = NextEphemeralInstanceId--;

		PS->AddCardToHand(Card);
		++Granted;
	}

	if (Granted > 0)
	{
		UE_LOG(LogBGCard, Log, TEXT("[카드지급] 좌석 %d 에 %s %d장 (요청 %d, %s)"),
			PS->GetSeatIndex(), *RowName.ToString(), Granted, Count,
			bEphemeral ? TEXT("일회용") : TEXT("일반"));
	}
	return Granted;
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

	//	손패 상한에 걸리면 덜 뽑힌다. 턴 진행은 그대로다 — 못 뽑았다고 턴이 멈추지 않는다.
	const int32 Drawn = DrawCardsToHand(PS, DrawNum);

	//	DeckCount 는 Player 것뿐이다. 봇(Enemy) 턴에 그대로 찍으면 안 바뀐 값이라 오해를 산다.
	const EBGDeckSide Side = GetDeckSideForSeat(CurrentSeat);
	const int32 RemainingDeck = (Side == EBGDeckSide::Enemy) ? EnemyDeckCount : DeckCount;

	UE_LOG(LogBGCard, Log, TEXT("[Turn] 좌석 %d 뽑기 %d/%d 장 (손패 %d, 덱 %d, Side=%d)"),
		CurrentSeat, Drawn, DrawNum, PS->GetHandCount(), RemainingDeck, (int32)Side);

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

	//	[비활성] 예전에는 손패가 한도(=체력)를 넘으면 Phase.Discard 로 보내 버릴 때까지
	//	  턴을 넘기지 않았다. 상한을 드로우 시점에 막는 지금은 초과 상태 자체가 생기지 않는다.
	//
	//	  ⚠️ 다만 훔치기(StealTargetRandom)로는 상한을 넘을 수 있다 — 뺏은 카드를 도로
	//	     버리게 하는 것은 그 카드의 의도와 어긋나므로 막지 않았다. 그래서 여기서도
	//	     버리기를 강요하지 않는다. 넘긴 손패는 다음 드로우가 막히는 것으로 정산된다.
	//
	//	  되살리려면 아래 블록의 주석을 풀고 GetHandLimitForSeat 이 체력을 돌려주게 하면 된다.
	//
	//	const int32 HandNum = PS ? PS->GetHandCount() : 0;
	//	const int32 Limit   = GetHandLimitForSeat(CurrentSeat);
	//	if (HandNum > Limit) { SetPhaseTag(BG::Phase::Discard.GetTag()); return; }

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

	//	손패 정리는 하지 않는다 — 상한은 드로우 시점에 막고, 훔치기로 넘긴 손패는
	//	버리게 하지 않는 것이 RequestEndTurn 과 같은 규칙이다.
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

	//	사망은 **반드시 대기열을 거친다.** 여기서 규칙을 바로 돌리면 화면에서는 아직 공격
	//	연출이 도는 중인데 카드가 버려지고 승패까지 나 버린다 — 연출이 결과를 "보여주기 전에"
	//	결과가 확정되는 셈이다. 연출 중이 아닌 사망(다이너마이트·밧줄)도 같은 줄에 세워
	//	"사망은 언제나 사망 연출을 거친다" 를 한 경로로 보장한다.
	//
	//	같은 좌석이 두 번 들어오지 않는다 — ABGCharacter::NotifyHealthDepleted 의
	//	bDead 플래그가 사망 통지 자체를 1회로 막는다. 그래도 방어적으로 확인한다.
	const bool bAlreadyQueued = PendingDeaths.ContainsByPredicate(
		[DeadSeat](const FBGPendingDeath& D) { return D.DeadSeat == DeadSeat; });

	if (!bAlreadyQueued && ActiveDeath.DeadSeat != DeadSeat)
	{
		PendingDeaths.Add({ DeadSeat, KillerSeat });
		UE_LOG(LogBGCard, Log, TEXT("[Death] 좌석 %d 사망 접수 (대기 %d 건)"),
			DeadSeat, PendingDeaths.Num());
	}

	//	연출 중이면 그 연출이 끝나는 자리에서 이어서 재생된다(EndCardCinematic / FinishDeathCinematic).
	if (IsCinematicPlaying())
	{
		return;
	}

	//	⚠️ 여기서 곧바로 열지 않는다. 사망은 GA 가 효과를 적용하는 **도중에** 통지되는데,
	//	   그 카드의 연출은 HandlePlayCard 의 마지막 줄에서야 열린다. 지금 열면 사망 연출이
	//	   먼저 서 버려 BeginCardCinematic 이 "이미 연출 중" 으로 튕기고, 공격·피격 연출 없이
	//	   사망 연출만 재생된다.
	//
	//	   한 틱 미루면 그 사이에 카드 처리가 전부 끝나 있다 — 연출 카드면 이미 Primary 가
	//	   서 있어 아래 검사에 걸리고(카드 연출이 끝날 때 이어받는다), 연출 없는 카드나
	//	   카드와 무관한 사망(밧줄 만료 등)이면 그대로 사망 연출이 열린다.
	//	   호출자마다 "지금이 카드 처리 중인가" 를 알 필요가 없어지는 것이 이 방식의 값어치다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (!IsCinematicPlaying())
				{
					TryBeginNextDeathCinematic();
				}
			}));
	}
}

bool ABGGameState::TryBeginNextDeathCinematic()
{
	if (!HasAuthority())
	{
		return false;
	}

	while (!PendingDeaths.IsEmpty())
	{
		//	판이 끝났으면 남은 사망은 버린다. 규칙적으로 이미 의미가 없고(보상도 승패도 확정),
		//	처리하면 종료 연출이 도는 동안 덱에서 카드가 빠진다.
		//	죽지 않은 사람의 역할도 CheckWinCondition 이 전원 공개하므로 잃는 정보가 없다.
		if (!bMatchRunning)
		{
			UE_LOG(LogBGCard, Log, TEXT("[Death] 판 종료 — 남은 사망 %d 건을 버립니다."),
				PendingDeaths.Num());

			//	연출은 버려도 "쓰러진 모습" 은 남겨야 한다. 안 하면 종료 화면에서
			//	죽은 사람이 멀쩡히 서 있다.
			for (const FBGPendingDeath& Skipped : PendingDeaths)
			{
				RevealDeath(Skipped.DeadSeat);
			}

			PendingDeaths.Reset();
			return false;
		}

		const FBGPendingDeath Death = PendingDeaths[0];
		PendingDeaths.RemoveAt(0);

		//	연출에 비출 폰을 고른다. 역참조가 비면 두고 간 폰을 쓴다.
		ABGPlayerState* DeadPS = GetPlayerStateBySeat(Death.DeadSeat);

		APawn* DeadPawn = DeadPS ? DeadPS->GetPawn() : nullptr;
		if (!DeadPawn && DeadPS)
		{
			DeadPawn = DeadPS->GetAbandonedPawn();
		}

		ABGCharacter* DeadChar = Cast<ABGCharacter>(DeadPawn);

		const bool bHasLength = (DeathPrimarySeconds > 0.f || DeathSecondarySeconds > 0.f);

		if (!DeadChar || !bHasLength)
		{
			UE_LOG(LogBGCard, Log, TEXT("[Death] 좌석 %d 사망 연출 생략 (%s) — 규칙만 처리합니다."),
				Death.DeadSeat, DeadChar ? TEXT("길이 0") : TEXT("캐릭터 없음"));

			RevealDeath(Death.DeadSeat);
			RevealDeadRole(Death.DeadSeat);
			ProcessSeatDeath(Death.DeadSeat, Death.KillerSeat);
			continue;   // 다음 사망자를 본다
		}

		ActiveDeath = Death;

		//	사망 연출에는 "대상" 이 없다. 두 단계 모두 사망자를 비춘다(GetCinematicSubject).
		CinematicInstigator = DeadChar;
		CinematicTarget     = nullptr;
		CinematicResolution = FBGCardResolution();

		UE_LOG(LogBGCard, Log, TEXT("[Death] 좌석 %d 사망 연출 시작 (남은 대기 %d 건)"),
			Death.DeadSeat, PendingDeaths.Num());

		SetCinematicStage(EBGCinematicStage::DeathPrimary);
		return true;
	}

	return false;
}

void ABGGameState::FinishDeathCinematic()
{
	//	먼저 꺼낸다. ProcessSeatDeath 안에서 연쇄 사망이 나면 HandleSeatDeath 가
	//	ActiveDeath 와 좌석을 비교하므로, 비우기 전에 부르면 그 사망이 중복으로 걸린다.
	const FBGPendingDeath Death = ActiveDeath;
	ActiveDeath = FBGPendingDeath();

	if (Death.DeadSeat != INDEX_NONE)
	{
		//	"역할 공개에 따른 결과" — 카드 정리·보상/벌칙·승패·턴이 전부 여기서 난다.
		ProcessSeatDeath(Death.DeadSeat, Death.KillerSeat);
	}

	//	다음 사망자로 곧장 넘어간다. 없으면 연출을 닫는다.
	if (!TryBeginNextDeathCinematic())
	{
		SetCinematicStage(EBGCinematicStage::None);
	}
}

void ABGGameState::RevealDeath(int32 DeadSeat)
{
	if (!HasAuthority() || DeadSeat == INDEX_NONE)
	{
		return;
	}

	if (ABGPlayerState* PS = GetPlayerStateBySeat(DeadSeat))
	{
		PS->SetDeathShown();
	}
}

void ABGGameState::RevealDeadRole(int32 DeadSeat)
{
	if (!HasAuthority() || DeadSeat == INDEX_NONE)
	{
		return;
	}

	//	뱅 규칙: 죽으면 역할이 공개된다. 이 시점부터 전원이 볼 수 있다.
	if (ABGPlayerState* PS = GetPlayerStateBySeat(DeadSeat))
	{
		PS->SetPublicRole(GetSeatRole(DeadSeat));
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

	//	두고 간 폰은 파괴하지 않는다 — 시체로 남겨 좌석·직업을 계속 볼 수 있게 한다.

	// ── 1. 사망자의 손패·장비를 전부 버린 패로 ──
	//	쓰러짐과 역할 공개는 여기가 아니라 사망 연출의 각 단계 진입 시점에 이미 끝나 있다.
	//	그래도 한 번 더 부른다 — 연출을 건너뛴 경로로 들어와도 둘 다 반드시 반영되게.
	//	(둘 다 1회 걸쇠가 있어 중복 호출은 무해하다)
	RevealDeath(DeadSeat);
	RevealDeadRole(DeadSeat);

	if (ABGPlayerState* DeadPS = GetPlayerStateBySeat(DeadSeat))
	{
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
			// 무법자를 잡으면 현상금 3장. 손패 상한에 걸리면 그만큼만 받는다.
			ABGPlayerState* KillerPS = GetPlayerStateBySeat(KillerSeat);
			const int32 Drawn = DrawCardsToHand(KillerPS, OutlawKillReward);
			UE_LOG(LogBGCard, Log, TEXT("[Death] 무법자 사살 보상 — 좌석 %d 가 %d/%d 장 획득"),
				KillerSeat, Drawn, OutlawKillReward);
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

void ABGGameState::ShuffleDeck(EBGDeckSide Side)
{
	TArray<FBGCardInstance>& TargetDeck = (Side == EBGDeckSide::Enemy) ? EnemyDeck : Deck;
	FRandomStream& Stream = (Side == EBGDeckSide::Enemy) ? EnemyRandomStream : RandomStream;

	for (int i = TargetDeck.Num() - 1; i > 0; --i)
	{
		int j = Stream.RandRange(0, i);
		TargetDeck.Swap(i, j);
	}
}

void ABGGameState::ReshuffleDeck(EBGDeckSide Side)
{	//	버림덱의 맨 위 카드(DiscardTop)를 제외하고 셔플하여 기존 덱에 추가
	TArray<FBGCardInstance>& TargetDeck    = (Side == EBGDeckSide::Enemy) ? EnemyDeck    : Deck;
	TArray<FBGCardInstance>& TargetDiscard = (Side == EBGDeckSide::Enemy) ? EnemyDiscard : Discard;
	FRandomStream& Stream = (Side == EBGDeckSide::Enemy) ? EnemyRandomStream : RandomStream;

	if (TargetDiscard.IsEmpty())
	{
		return;
	}

	for (int i = TargetDiscard.Num() - 1; i > 0; --i)
	{
		int j = Stream.RandRange(0, i);
		TargetDiscard.Swap(i, j);
	}

	UE_LOG(LogBGCard, Log, TEXT("[ReshuffleDeck] 버린 패 %d 장을 덱으로 되돌립니다 (Side=%d)."),
		TargetDiscard.Num(), (int32)Side);

	TargetDeck.Append(TargetDiscard);
	TargetDiscard.Empty();
	RefreshPublicCounts();
}


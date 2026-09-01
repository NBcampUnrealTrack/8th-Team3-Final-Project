// BGAIBrainComponent.cpp


#include "AI/BGAIBrainComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BGGameplayTags.h"
#include "AI/BGAIPlayerController.h"
#include "Card/BGCardType.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "Engine/GameInstance.h"
#include "GAS/BGAttributeSet.h"

UBGAIBrainComponent::UBGAIBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UBGAIBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (ABGGameState* GS = GetBGGameState())
	{
		GS->OnTurnStateChanged.AddDynamic(this, &UBGAIBrainComponent::HandleTurnStateChanged);
	}
}

void UBGAIBrainComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ABGGameState* GS = GetBGGameState())
	{
		GS->OnTurnStateChanged.RemoveDynamic(this, &UBGAIBrainComponent::HandleTurnStateChanged);
	}

	GetWorld()->GetTimerManager().ClearTimer(ActionTimerHandle);

	Super::EndPlay(Reason);
}

void UBGAIBrainComponent::HandleTurnStateChanged()
{
	const ABGAIPlayerController* AIPC = GetOwnerAIController();
	const ABGGameState* GS = GetBGGameState();
	if (!AIPC || !GS)
	{
		return;
	}

	const int32 MySeat = AIPC->GetPlayerState<ABGPlayerState>()->GetSeatIndex();
	if (GS->GetCurrentSeat() != MySeat || GS->GetPhaseTag() != BG::Phase::Play.GetTag())
	{
		return; // 내 턴의 Play 페이즈가 아니면 return;
	}

	// 내 턴의 Play 페이즈로 새로 들어온 시점이다 — 이번 턴의 거부 기록을 비운다.
	RejectedThisTurn.Empty();

	TryPlayNextCard();
}

void UBGAIBrainComponent::TryPlayNextCard()
{
	ABGAIPlayerController* AIPC = GetOwnerAIController();
	ABGPlayerState* PS = AIPC->GetPlayerState<ABGPlayerState>();
	ABGGameState* GS = GetBGGameState();

	const int32 MySeat = PS->GetSeatIndex();

	// 이미 내 턴이 아니면(RequestEndTurn 이 뒤늦게 성사됐거나 등) 재시도를 멈춘다.
	if (GS->GetCurrentSeat() != MySeat || GS->GetPhaseTag() != BG::Phase::Play.GetTag())
	{
		return;
	}

	// 아직 내 턴이지만 연출(Cinematic) 재생 중이면 카드를 고르지 않는다.
	if (!GS->CanSeatPlayCards(MySeat))
	{
		GetWorld()->GetTimerManager().SetTimer(
			ActionTimerHandle, this, &UBGAIBrainComponent::TryPlayNextCard, MinActionInterval, false);
		return;
	}

	const int32 Before = PS->GetCardsUsedThisTurn();

	int32 InstanceId, TargetSeat;
	if (!SelectCardToPlay(InstanceId, TargetSeat))
	{
		//	턴이 이미 끝남. 재시도 타이머까지 걸면 다음 턴 드로우 전에 낡은 손패로 판단하게 된다.
		GS->RequestEndTurn(MySeat); // 규칙 4: 낼 카드 없음
		return;
	}

	AIPC->HandlePlayCard(InstanceId, TargetSeat);

	if (PS->GetCardsUsedThisTurn() == Before)
	{
		// 게이트에 걸려 거부됨 — 같은 카드를 이번 턴에 다시 시도하지 않는다.
		RejectedThisTurn.Add(InstanceId);
	}

	GetWorld()->GetTimerManager().SetTimer(
		ActionTimerHandle,
		this,
		&UBGAIBrainComponent::TryPlayNextCard,
		MinActionInterval,
		false
	);
}

bool UBGAIBrainComponent::SelectCardToPlay(int32& OutInstanceId, int32& OutTargetSeat) const
{
	ABGAIPlayerController* AIPC = GetOwnerAIController();
	ABGPlayerState* PS = AIPC->GetPlayerState<ABGPlayerState>();
	ABGGameState* GS = GetBGGameState();
	UBGDataSubsystem* DataSys = AIPC->GetGameInstance()->GetSubsystem<UBGDataSubsystem>();

	const int32 MySeat = PS->GetSeatIndex();

	// 사용 한도 이미 소진 - 더 볼 것도 없이 규칙 4
	if (!GS->HasCardUsesLeft(MySeat))
	{
		return false;
	}

	const int32 MyHealth = GS->GetSeatHealth(MySeat);
	const UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn());
	const int32 MyMaxHealth = ASC
		? FMath::RoundToInt(ASC->GetNumericAttribute(UBGAttributeSet::GetMaxHealthAttribute()))
		: MyHealth;

	const TArray<FBGCardInstance>& Hand = PS->GetHand();
	auto IsCandidate = [this](const FBGCardInstance& Card)
	{
		return !RejectedThisTurn.Contains(Card.InstanceId);
	};

	// 규칙 1: 체력 ≤ 최대/2 + 회복 카드
	if (MyHealth * 2 <= MyMaxHealth)
	{
		for (const FBGCardInstance& Card : Hand)
		{
			if (!IsCandidate(Card)) continue;
			bool bHeal, bAttack, bNeedsTarget;
			ClassifyCard(DataSys, Card, bHeal, bAttack, bNeedsTarget);
			if (bHeal)
			{
				OutInstanceId = Card.InstanceId;
				OutTargetSeat = INDEX_NONE;
				return true;
			}
		}
	}

	// 규칙 2: 공격 카드
	for (const FBGCardInstance& Card : Hand)
	{
		if (!IsCandidate(Card)) continue;
		bool bHeal, bAttack, bNeedsTarget;
		ClassifyCard(DataSys, Card, bHeal, bAttack, bNeedsTarget);
		if (bAttack)
		{
			OutInstanceId = Card.InstanceId;
			OutTargetSeat = PickRandomTargetSeat();
			return true;
		}
	}

	// 규칙 3: 그 밖에 쓸 수 있는 카드 무작위 1장
	TArray<const FBGCardInstance*> Rest;
	for (const FBGCardInstance& Card : Hand)
	{
		if (IsCandidate(Card)) Rest.Add(&Card);
	}
	if (Rest.Num() > 0)
	{
		const FBGCardInstance* Pick = Rest[FMath::RandRange(0, Rest.Num() - 1)];
		bool bHeal, bAttack, bNeedsTarget;
		ClassifyCard(DataSys, *Pick, bHeal, bAttack, bNeedsTarget);

		OutInstanceId = Pick->InstanceId;
		OutTargetSeat = bNeedsTarget ? PickRandomTargetSeat() : INDEX_NONE;
		return true;
	}

	return false; // 규칙 4: 없음
}

void UBGAIBrainComponent::ClassifyCard(const UBGDataSubsystem* DataSys, const FBGCardInstance& Card,
	bool& bOutHeal, bool& bOutAttack, bool& bOutNeedsTarget) const
{
	const FBGCardRow* Row = DataSys ? DataSys->GetCardRow(Card.CardId) : nullptr;
	bOutHeal = bOutAttack = bOutNeedsTarget = false;
	if (!Row)
	{
		return;
	}

	bOutAttack = Row->TraitTags.HasTag(BG::Card::Trait::Attack.GetTag());
	bOutNeedsTarget = Row->TraitTags.HasTag(BG::Card::Trait::NeedsTargetSeat.GetTag());

	for (const FBGCardEffect& Eff : Row->OutcomeEffects.Success)
	{
		if (Eff.Op == EBGCardEffectOp::HealSelf) { bOutHeal = true; break; }
	}
	if (!bOutHeal)
	{
		for (const FBGCardEffect& Eff : Row->OutcomeEffects.CriticalSuccess)
		{
			if (Eff.Op == EBGCardEffectOp::HealSelf) { bOutHeal = true; break; }
		}
	}
}

int32 UBGAIBrainComponent::PickRandomTargetSeat() const
{
	ABGPlayerState* PS = GetOwnerAIController()->GetPlayerState<ABGPlayerState>();
	TArray<int32> Candidates = GetBGGameState()->GetAliveSeatsInTableOrder();
	Candidates.RemoveSingle(PS->GetSeatIndex());
	return Candidates.Num() > 0 ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)] : INDEX_NONE;
}

ABGAIPlayerController* UBGAIBrainComponent::GetOwnerAIController() const
{
	return Cast<ABGAIPlayerController>(GetOwner());
}

ABGGameState* UBGAIBrainComponent::GetBGGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
}
